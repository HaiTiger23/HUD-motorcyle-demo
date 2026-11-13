#include "hud_settings.h"
#include "phone_link.h"
#include "ai_voice.h"
#include "wallpaper.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <nvs.h>
#include <algorithm>
#include <atomic>

namespace {
constexpr size_t MAX_APPS = 24;
struct AppRule { char id[96]; bool enabled; };
struct Stored { uint32_t version; HudSettings settings; uint8_t count; AppRule apps[MAX_APPS]; } state{};
struct Request { uint16_t peer; uint8_t size; uint8_t bytes[100]; };
QueueHandle_t requests;
SemaphoreHandle_t snapshotGuard;
NimBLECharacteristic* channel;
uint8_t snapshot[7];
uint8_t hardwareSnapshot[3];
uint8_t lastReply[160];
uint8_t lastReplyLength=0;
uint32_t revision = 0, dirtyAt = 0;
Preferences preferences;
HudHardwareSettings hardware;
bool autoBrightness=true;
char ownerPin[5]={};
std::atomic<uint32_t> rpcReceived{0},rpcQueued{0},rpcProcessed{0},rpcNotified{0};
std::atomic<uint32_t> rpcLast{0};
std::atomic<uint16_t> rpcSubscription{0};
struct AppAlias { char id[96]; char name[96]; } aliases[32];
uint8_t aliasCount=0, aliasCursor=0;
bool aliasesDirty=false;
uint32_t aliasesChangedAt=0;
void publish() {
    const auto& s = state.settings;
    xSemaphoreTake(snapshotGuard, portMAX_DELAY);
    const uint8_t bytes[] = {1, s.theme, s.face, uint8_t(s.timeoutSeconds),
        uint8_t(s.timeoutSeconds >> 8), uint8_t(s.allowNewApps), state.count};
    memcpy(snapshot, bytes, sizeof(snapshot));
    hardwareSnapshot[0]=hardware.brightness; hardwareSnapshot[1]=hardware.speakerVolume; hardwareSnapshot[2]=hardware.rotation;
    xSemaphoreGive(snapshotGuard);
    ++revision;
}
// Faces 0 Số lớn, 2 Kim hiện đại, 5 Kim cổ điển, 6 Đi xe, 8 iPhone (full-screen lock style), 9 Infograph.
// Retired faces map onto their replacement: 1/3/4 -> 0 (big digits), 7 (speed dial) -> 6 (ride).
// The colour/photo background is separate and applies to every face.
uint8_t normalizeFace(uint8_t face) {
    if (face == 1 || face == 3 || face == 4) return 0;
    if (face == 7) return 6;
    return face;
}
bool validTimeout(uint16_t value) {
    return value == 0 || value == 15 || value == 30 || value == 60 || value == 120 || value == 300;
}
int findApp(const char* id) {
    for (unsigned i = 0; i < state.count; ++i) if (!strcmp(state.apps[i].id, id)) return i;
    return -1;
}
void changed() { dirtyAt = millis() | 1; publish(); }
// NVS writes the new blob before freeing the old one, so a nearly full partition
// rejects the 2.3 KB state. On failure, free the old copy first and retry.
bool saveState() {
    if (preferences.putBytes("state", &state, sizeof(state)) == sizeof(state)) return true;
    preferences.remove("state");
    const bool ok = preferences.putBytes("state", &state, sizeof(state)) == sizeof(state);
    Serial.printf("Settings: NVS retry after removing old state: %s\n", ok ? "ok" : "failed");
    return ok;
}
class ChannelCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic*,NimBLEConnInfo&,uint16_t value) override { rpcSubscription=value; }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        uint8_t value[171];
        xSemaphoreTake(snapshotGuard, portMAX_DELAY);
        memcpy(value,snapshot,7); memcpy(value+7,hardwareSnapshot,3);
        value[10]=lastReplyLength; memcpy(value+11,lastReply,lastReplyLength);
        ch->setValue(value,11+lastReplyLength);
        xSemaphoreGive(snapshotGuard);
    }
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo& peer) override {
        ++rpcReceived;
        if (!peer.isEncrypted()) return;
        const auto value = ch->getValue();
        if(value.size()) rpcLast=(value[0]<<16)|value.size();
        if (value.size() < 2 || value.size() > 99) return;
        Request request{}; request.peer = peer.getConnHandle(); request.size = value.size();
        memcpy(request.bytes, value.data(), value.size());
        // No flash writes or LVGL calls on the Bluetooth callback thread.
        if (xQueueSend(requests, &request, 0) != pdTRUE) {
            const uint8_t error[] = {uint8_t(value[0] | 0x80), value[1], 3};
            ch->notify(error,sizeof(error),peer.getConnHandle());
        } else ++rpcQueued;
    }
} callbacks;
}
void hudSettingsBegin() {
    requests = xQueueCreate(8, sizeof(Request));
    snapshotGuard = xSemaphoreCreateMutex();
    preferences.begin("hud-settings", false);
    uint8_t savedHardware[4]{};
    if(preferences.getBytesLength("hardware")==4 && preferences.getBytes("hardware",savedHardware,4)==4 &&
       savedHardware[0]==1 && savedHardware[1]>=5 && savedHardware[1]<=100 && savedHardware[2]<=100 && (savedHardware[3]==0 || savedHardware[3]==2))
        hardware={savedHardware[1],savedHardware[2],savedHardware[3]};
    // Not part of the condition above.
    autoBrightness=preferences.getUChar("autobright",1)!=0;
    if(preferences.getString("ownerpin",ownerPin,sizeof(ownerPin))!=4) ownerPin[0]=0;
    // Read with the raw NVS API: when the blob is damaged, Preferences::getBytes logs
    // nvs_error() == nullptr through printf and panics, which boot-looped the board.
    {
        nvs_handle_t handle;
        if (nvs_open("hud-settings", NVS_READWRITE, &handle) == ESP_OK) {
            size_t size = sizeof(state);
            const esp_err_t err = nvs_get_blob(handle, "state", &state, &size);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                Serial.printf("Settings: stored state unreadable (0x%x); resetting\n", unsigned(err));
                nvs_erase_key(handle, "state"); nvs_commit(handle);
                state = {};
            } else if (err == ESP_OK && size != sizeof(state)) state = {};
            nvs_close(handle);
        }
    }
    bool valid = state.version == 1 && state.count <= MAX_APPS && state.settings.theme < 8 &&
        state.settings.face < 10 && validTimeout(state.settings.timeoutSeconds);
    if (valid) for (unsigned i = 0; i < state.count; ++i)
        valid = valid && state.apps[i].id[0] && memchr(state.apps[i].id, 0, sizeof(state.apps[i].id));
    if (valid && normalizeFace(state.settings.face) != state.settings.face) {
        state.settings.face = normalizeFace(state.settings.face); saveState();
    }
    if (!valid) {
        state = {}; state.version = 1; state.settings = HudSettings{};
        const char* defaults[] = {"com.apple.MobileSMS", "com.apple.mobilephone", "com.facebook.Messenger",
            "net.whatsapp.WhatsApp", "com.vng.zalo", "com.google.Maps", "com.apple.Maps"};
        for (const auto id : defaults) {
            auto& app = state.apps[state.count++]; strlcpy(app.id, id, sizeof(app.id)); app.enabled = true;
        }
    }
    publish();
}
void hudSettingsAttach(NimBLEService* service) {
    channel = service->createCharacteristic("ee43a103-1b32-4d0f-9ab8-2248b0a01000",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY);
    channel->setCallbacks(&callbacks);
}
void hudSettingsTick() {
    Request request{};
    while (xQueueReceive(requests, &request, 0) == pdTRUE) {
        ++rpcProcessed;
        const Stored previous = state;
        const auto* b = request.bytes;
        uint8_t reply[160] = {uint8_t(b[0] | 0x80), b[1], 1};
        size_t length = 3;
        if((b[0]==0x14 && request.size==2) || (b[0]==0x15 && request.size==5)) {
            bool valid=true;
            if(b[0]==0x15) {
                valid=b[2]>=5 && b[2]<=100 && b[3]<=100 && (b[4]==0 || b[4]==2);
                const uint8_t stored[]={1,b[2],b[3],b[4]};
                if(valid) valid=preferences.putBytes("hardware",stored,4)==4;
                if(valid) {
                    if(b[2]!=hardware.brightness && autoBrightness) hudSetAutoBrightness(false);
                    hardware={b[2],b[3],b[4]}; publish();
                }
            }
            if(valid) { reply[2]=0; reply[3]=1; reply[4]=hardware.brightness; reply[5]=hardware.speakerVolume; reply[6]=hardware.rotation; length=7; }
        } else if(b[0]==0x17 && (request.size==2 || request.size==3 || request.size==6)) {
            // 0x17 query / [0] clear / 4 ASCII digits set. The PIN itself is never sent back.
            bool valid=true;
            if(request.size==3) valid=b[2]==0 && (preferences.remove("ownerpin"), true);
            if(request.size==6) {
                for(int i=2;i<6;++i) valid=valid && b[i]>='0' && b[i]<='9';
                char pin[5]={char(b[2]),char(b[3]),char(b[4]),char(b[5]),0};
                if(valid) valid=preferences.putString("ownerpin",pin)==4;
                if(valid) strlcpy(ownerPin,pin,sizeof(ownerPin));
            }
            if(request.size==3 && valid) ownerPin[0]=0;
            if(valid) { reply[2]=0; reply[3]=ownerPin[0] ? 1 : 0; length=4; }
        } else if(b[0]==0x18 && (request.size==2 || request.size==3 || request.size==4 || request.size==5)) {
            // Background: query / [0] remove the photo / [0x0B, on] photo background / [r,g,b] clock colour.
            bool valid=request.size!=3 || b[2]==0;
            if(request.size==3 && b[2]==0) { wallpaperClear(); wallpaperSetBackground(false); }
            if(request.size==4) { valid=b[2]==0x0B; if(valid) wallpaperSetBackground(b[3]!=0); }
            if(request.size==5) wallpaperSetColor(uint32_t(b[2])<<16 | uint32_t(b[3])<<8 | b[4]);
            if(valid) {
                const uint32_t c=wallpaperColor();
                reply[2]=0; reply[3]=uint8_t(c>>16); reply[4]=uint8_t(c>>8); reply[5]=uint8_t(c);
                reply[6]=wallpaperHas(); reply[7]=wallpaperBackground(); length=8;
            }
        } else if(b[0]==0x16 && request.size==2) {
            reply[2]=aiVoiceTestSpeaker() ? 0 : 3;
        } else if (b[0] == 0x10 && request.size == 7) {
            const uint16_t seconds = b[4] | uint16_t(b[5]) << 8;
            if (b[2] < 8 && b[3] < 10 && validTimeout(seconds) && b[6] < 2) {
                state.settings.theme = b[2]; state.settings.face = normalizeFace(b[3]);
                state.settings.timeoutSeconds = seconds; state.settings.allowNewApps = b[6]; changed();
                reply[2] = 0; memcpy(reply + 3, snapshot, 7); length = 10;
            }
        } else if ((b[0] == 0x11 || b[0] == 0x13) && request.size == 3 && b[2] < state.count) {
            const auto& app = state.apps[b[2]];
            reply[2] = 0; reply[3] = b[2]; reply[4] = app.enabled;
            length = 5 + strlen(app.id); memcpy(reply + 5, app.id, length - 5);
            if(b[0]==0x13) {
                reply[length++]=0;
                const char* name=hudSettingsAppName(app.id);
                if(!name[0]) phoneLinkRequestAppName(app.id);
                size_t size=std::min(size_t(55),strlen(name));
                while(size && (uint8_t(name[size])&0xc0)==0x80) --size;
                memcpy(reply+length,name,size); length+=size;
            }
        } else if (b[0] == 0x12 && request.size >= 4 && b[2] < 2) {
            char id[96]{};
            const size_t size = request.size - 3;
            bool valid = size < sizeof(id);
            for (size_t i = 0; i < size; ++i) valid = valid &&
                ((b[i+3] >= 'a' && b[i+3] <= 'z') || (b[i+3] >= 'A' && b[i+3] <= 'Z') ||
                 (b[i+3] >= '0' && b[i+3] <= '9') || b[i+3] == '.' || b[i+3] == '-' || b[i+3] == '_');
            if (valid) {
                memcpy(id, b + 3, size); int index = findApp(id);
                if (index < 0 && state.count < MAX_APPS) index = state.count++;
                if (index >= 0) {
                    strlcpy(state.apps[index].id, id, sizeof(id));
                    state.apps[index].enabled = b[2]; changed(); reply[2] = 0;
                } else reply[2] = 2;
            }
        }
        if (reply[2] == 0 && (b[0] == 0x10 || b[0] == 0x12)) {
            if (saveState()) dirtyAt = 0;
            else {
                state = previous; publish(); reply[2] = 4; length = 3;
                Serial.println("Settings: flash write failed; change rolled back");
            }
        }
        // Explicit payload bypasses onRead(), which intentionally exposes the
        // settings snapshot and would otherwise replace this RPC response.
        xSemaphoreTake(snapshotGuard,portMAX_DELAY);
        lastReplyLength=length; memcpy(lastReply,reply,length);
        xSemaphoreGive(snapshotGuard);
        if(channel->notify(reply,length,request.peer)) ++rpcNotified;
    }
    if (dirtyAt && uint32_t(millis() - dirtyAt) > 1000) {
        if (saveState()) dirtyAt = 0;
    }
    if(channel && aliasesDirty && uint32_t(millis()-aliasesChangedAt)>1200) {
        aliasesDirty=false; channel->notify(snapshot,sizeof(snapshot));
    }
}
const HudSettings& hudSettingsGet() { return state.settings; }
const HudHardwareSettings& hudHardwareSettingsGet() { return hardware; }
bool hudSettingsApply(const HudSettings& settings) {
    if(settings.theme>=8 || settings.face>=10 || !validTimeout(settings.timeoutSeconds)) return false;
    const HudSettings previous=state.settings;
    state.settings=settings; state.settings.face=normalizeFace(settings.face);
    if(!saveState()) {
        state.settings=previous; return false;
    }
    publish(); dirtyAt=0;
    if(channel) channel->notify(snapshot,sizeof(snapshot));
    return true;
}
bool hudHardwareSettingsApply(const HudHardwareSettings& value) {
    if(value.brightness<5 || value.brightness>100 || value.speakerVolume>100 || (value.rotation!=0 && value.rotation!=2)) return false;
    const uint8_t stored[]={1,value.brightness,value.speakerVolume,value.rotation};
    if(preferences.putBytes("hardware",stored,4)!=4) return false;
    hardware=value; publish();
    if(channel) channel->notify(snapshot,sizeof(snapshot));
    return true;
}
uint32_t hudSettingsRevision() { return revision; }
bool hudSettingsAllowNotification(const char* app) {
    const int index = findApp(app);
    if (index >= 0) return state.apps[index].enabled;
    if (app[0] && strlen(app) < 96 && state.count < MAX_APPS) {
        auto& rule = state.apps[state.count++]; strlcpy(rule.id, app, sizeof(rule.id));
        rule.enabled = state.settings.allowNewApps; changed();
    }
    return state.settings.allowNewApps;
}
void hudSettingsPrintStatus() {
    Serial.printf("Settings RPC: received=%u; queued=%u; processed=%u; notified=%u; last=%08X; subscribed=%u\n",rpcReceived.load(),rpcQueued.load(),rpcProcessed.load(),rpcNotified.load(),rpcLast.load(),unsigned(rpcSubscription.load()));
    Serial.printf("Hardware: brightness=%u; speaker=%u; rotation=%u\n",hardware.brightness,hardware.speakerVolume,unsigned(hardware.rotation)*90);
    Serial.printf("Settings: theme=%u; face=%u; timeout=%us; apps=%u; new=%u; saved=%u\n",
        state.settings.theme, state.settings.face, state.settings.timeoutSeconds,
        state.count, state.settings.allowNewApps, !dirtyAt);
    Serial.printf("App display names: resolved=%u\n",aliasCount);
}
const char* hudSettingsAppName(const char* id) {
    for(unsigned i=0;i<aliasCount;++i) if(!strcmp(aliases[i].id,id)) return aliases[i].name;
    return "";
}
void hudSettingsSetAppName(const char* id,const char* name) {
    if(!id[0] || !name[0]) return;
    int index=-1;
    for(unsigned i=0;i<aliasCount;++i) if(!strcmp(aliases[i].id,id)) index=i;
    if(index>=0 && !strcmp(aliases[index].name,name)) return;
    if(index<0) index=aliasCount<32 ? aliasCount++ : aliasCursor++%32;
    strlcpy(aliases[index].id,id,sizeof(aliases[index].id));
    strlcpy(aliases[index].name,name,sizeof(aliases[index].name));
    aliasesDirty=true; aliasesChangedAt=millis();
}
void hudSettingsRequestAppNames() {
    for(unsigned i=0;i<state.count;++i)
        if(!hudSettingsAppName(state.apps[i].id)[0]) phoneLinkRequestAppName(state.apps[i].id);
}
bool hudAutoBrightness() { return autoBrightness; }
void hudSetAutoBrightness(bool enabled) {
    if(enabled==autoBrightness) return;
    autoBrightness=enabled; preferences.putUChar("autobright",enabled ? 1 : 0); publish();
}
bool hudOwnerPinSet() { return ownerPin[0]!=0; }
bool hudOwnerPinMatches(const char* digits) { return ownerPin[0] && !strncmp(ownerPin,digits,4) && strlen(digits)==4; }
