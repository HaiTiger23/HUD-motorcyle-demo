#include "nav_stream.h"
#include "hud_settings.h"
#include "ai_voice.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <nav_protocol.h>
#include <esp_heap_caps.h>
#include <new>
#include <atomic>
#include <algorithm>

namespace {
uint8_t* front;
uint8_t* back;
hudnav::Receiver* receiver;
SemaphoreHandle_t guard;
NimBLECharacteristic* acknowledgement;
NimBLECharacteristic* incomingChannel;
NimBLEService* navigationService;
NimBLEServer* navigationServer;
uint32_t displayedFrame = 0;
uint32_t controlSent = 0;
bool ready = false;
bool cleared = false;
// Wallpaper: the phone sends [6] and then one 320x240 JPEG over the map transfer.
bool wallpaperNext = false, wallpaperReady = false;
uint8_t* wallpaper = nullptr;
size_t wallpaperSize = 0;
size_t readySize = 0;
uint32_t frames = 0, rejected = 0;
std::atomic<bool> externalGuidance{false};
std::atomic<int> speedTenths{-1};
std::atomic<uint32_t> speedAt{0};
class SpeedInput : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo& peer) override {
        if (!peer.isEncrypted()) return;
        const auto value=ch->getValue();
        if (value.size()!=2) return;
        const int speed=value[0] | (uint16_t(value[1])<<8);
        if (speed>3000 && speed!=65535) return;
        speedTenths=speed==65535 ? -1 : speed; speedAt=millis();
    }
} speedInput;
portMUX_TYPE infoLock = portMUX_INITIALIZER_UNLOCKED;
TripInfo trip;
WeatherInfo weather;
TodayDistance today;
uint32_t le32(const uint8_t* p) { return p[0] | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
uint16_t le16(const uint8_t* p) { return p[0] | uint16_t(p[1])<<8; }
class InfoInput : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo& peer) override {
        if (!peer.isEncrypted()) return;
        const auto value=ch->getValue();
        const uint8_t* b=value.data(); const size_t n=value.size();
        if (n>=12 && b[0]==1) {
            TripInfo next; next.active=true; next.receivedAt=millis()|1;
            next.remainingMetres=le32(b+1); next.turnMetres=le32(b+5);
            next.etaMinute=le16(b+9); next.maneuver=b[11];
            size_t size=std::min(n-12,sizeof(next.instruction)-1);
            while (size && 12+size<n && (b[12+size]&0xC0)==0x80) --size; // never cut inside a UTF-8 sequence
            memcpy(next.instruction,b+12,size);
            portENTER_CRITICAL(&infoLock); trip=next; portEXIT_CRITICAL(&infoLock);
        } else if (n==1 && b[0]==2) {
            portENTER_CRITICAL(&infoLock); trip=TripInfo{}; portEXIT_CRITICAL(&infoLock);
        } else if (n==9 && b[0]==4) {
            TodayDistance next; next.valid=true; next.metres=le32(b+1); next.dayKey=le32(b+5);
            portENTER_CRITICAL(&infoLock); today=next; portEXIT_CRITICAL(&infoLock);
        } else if (n==12 && b[0]==3) {
            WeatherInfo next; next.valid=true; next.receivedAt=millis()|1;
            next.rainChance=std::min<uint8_t>(100,b[1]); next.rainInMinutes=le16(b+2);
            next.tempTenths=int16_t(le16(b+4)); next.sunriseMinute=le16(b+6); next.sunsetMinute=le16(b+8);
            portENTER_CRITICAL(&infoLock); weather=next; portEXIT_CRITICAL(&infoLock);
        }
    }
} infoInput;
class Incoming : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& peer) override {
        if (!peer.isEncrypted() || !guard) return;
        const auto& value = characteristic->getValue();
        xSemaphoreTake(guard, portMAX_DELAY);
        if (value.size() == 2 && value[0] == 5 && value[1] < 2) {
            externalGuidance = value[1];
            xSemaphoreGive(guard); return;
        }
        if (value.size() == 1 && value[0] == 6) {
            receiver->reset(); receiver->expect(hudnav::WALL_WIDTH, hudnav::WALL_HEIGHT);
            wallpaperNext = true;
            xSemaphoreGive(guard); return;
        }
        if (value.size() == 1 && value[0] == 4) {
            receiver->reset(); ready = false; cleared = true;
            xSemaphoreGive(guard);
            return;
        }
        hudnav::Result result = hudnav::Result::Rejected;
        if (!ready) result = receiver->feed(value.data(), value.size());
        if (result == hudnav::Result::Complete && wallpaperNext) {
            // Kept apart from map frames: copy out and let the UI save it.
            if (wallpaper) { memcpy(wallpaper, back, receiver->size()); wallpaperSize = receiver->size(); wallpaperReady = true; }
            wallpaperNext = false; receiver->reset(); receiver->expect(hudnav::WIDTH, hudnav::HEIGHT);
        } else if (result == hudnav::Result::Complete) {
            readySize = receiver->size();
            ready = true;
            ++frames;
        } else if (result == hudnav::Result::Rejected) {
            ++rejected;
            if (wallpaperNext && value.size() == 5 && value[0] == 3) { wallpaperNext = false; receiver->expect(hudnav::WIDTH, hudnav::HEIGHT); }
        }
        if (value.size() == 5 && value[0] == 3) {
            const uint8_t reply[] = {uint8_t(result == hudnav::Result::Complete ? 0 : 1),
                value[1], value[2], value[3], value[4]};
            acknowledgement->setValue(reply, sizeof(reply));
            acknowledgement->notify(peer.getConnHandle());
        }
        xSemaphoreGive(guard);
    }
} incoming;
}
bool navStreamBegin(NimBLEServer* server) {
    navigationServer = server;
    front = static_cast<uint8_t*>(heap_caps_malloc(hudnav::MAX_IMAGE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    back = static_cast<uint8_t*>(heap_caps_malloc(hudnav::MAX_IMAGE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    guard = xSemaphoreCreateMutex();
    wallpaper = static_cast<uint8_t*>(heap_caps_malloc(hudnav::MAX_IMAGE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!front || !back || !guard) return false;
    receiver = new (std::nothrow) hudnav::Receiver(back, hudnav::MAX_IMAGE);
    if (!receiver) return false;
    auto service = server->createService("ee43a100-1b32-4d0f-9ab8-2248b0a01000");
    navigationService = service;
    hudSettingsAttach(service);
    aiVoiceAttach(server,service);
    auto speed=service->createCharacteristic("ee43a104-1b32-4d0f-9ab8-2248b0a01000",
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
    speed->setCallbacks(&speedInput);
    auto info=service->createCharacteristic("ee43a106-1b32-4d0f-9ab8-2248b0a01000",
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
    info->setCallbacks(&infoInput);
    auto rx = service->createCharacteristic("ee43a101-1b32-4d0f-9ab8-2248b0a01000",
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
    incomingChannel = rx;
    acknowledgement = service->createCharacteristic("ee43a102-1b32-4d0f-9ab8-2248b0a01000",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY);
    rx->setCallbacks(&incoming);
    return true;
}
void navStreamReset() {
    aiVoiceReset();
    if (!guard) return;
    xSemaphoreTake(guard, portMAX_DELAY);
    receiver->reset(); receiver->expect(hudnav::WIDTH, hudnav::HEIGHT);
    wallpaperNext = false;
    ready = false;
    cleared = true;
    externalGuidance = false;
    displayedFrame = 0;
    speedTenths=-1; speedAt=0;
    xSemaphoreGive(guard);
    portENTER_CRITICAL(&infoLock); trip=TripInfo{}; portEXIT_CRITICAL(&infoLock);
}
bool navStreamTake(const uint8_t*& bytes, size_t& size) {
    if (!guard) return false;
    xSemaphoreTake(guard, portMAX_DELAY);
    if (cleared) {
        cleared = false; bytes = nullptr; size = 0;
        xSemaphoreGive(guard); return true;
    }
    if (!ready) { xSemaphoreGive(guard); return false; }
    auto previous = front; front = back; back = previous;
    bytes = front; size = readySize;
    displayedFrame = receiver->id();
    *receiver = hudnav::Receiver(back, hudnav::MAX_IMAGE);
    ready = false;
    xSemaphoreGive(guard);
    return true;
}
void navStreamPrintStatus() {
    if (!guard) return;
    xSemaphoreTake(guard, portMAX_DELAY);
    Serial.printf("Navigation stream: frames=%u; rejected=%u; pending=%u\n", frames, rejected, ready);
    Serial.printf("Navigation guidance: external=%u\n", unsigned(externalGuidance.load()));
    Serial.printf("Navigation controls: sent=%u\n", controlSent);
    Serial.printf("Navigation GATT: channels=%u; RX=%u; TX=%u\n",
                  unsigned(navigationService->getCharacteristics().size()), incomingChannel->getHandle(),
                  acknowledgement->getHandle());
    xSemaphoreGive(guard);
}
bool navStreamExternalGuidance() { return externalGuidance.load(); }
int navStreamSpeedTenths() { return millis()-speedAt.load()>5000 ? -1 : speedTenths.load(); }
bool navStreamControl(uint8_t command, int16_t dx, int16_t dy) {
    if (!guard || !navigationServer || navigationServer->getConnectedCount()!=1 || command<1 || command>6 ||
        dx < -300 || dx > 300 || dy < -200 || dy > 200 || (command!=4 && (dx || dy))) return false;
    const auto peer = navigationServer->getPeerInfo(uint8_t(0));
    if (!peer.isEncrypted()) return false;
    xSemaphoreTake(guard, portMAX_DELAY);
    const uint32_t id=displayedFrame;
    xSemaphoreGive(guard);
    const uint8_t value[] = {0x80, command, uint8_t(id), uint8_t(id>>8), uint8_t(id>>16), uint8_t(id>>24),
        uint8_t(dx), uint8_t(uint16_t(dx)>>8), uint8_t(dy), uint8_t(uint16_t(dy)>>8)};
    const bool sent=acknowledgement->notify(value,sizeof(value),peer.getConnHandle());
    if(sent) ++controlSent;
    return sent;
}
bool navStreamTrip(TripInfo& out) {
    portENTER_CRITICAL(&infoLock); out=trip; portEXIT_CRITICAL(&infoLock);
    // The phone refreshes at least every 10 s while guiding.
    if (out.active && millis()-out.receivedAt>30000) out.active=false;
    return out.active;
}
bool navStreamWeather(WeatherInfo& out) {
    portENTER_CRITICAL(&infoLock); out=weather; portEXIT_CRITICAL(&infoLock);
    if (out.valid && millis()-out.receivedAt>3UL*3600*1000) out.valid=false;
    return out.valid;
}
bool navStreamTakeWallpaper(uint8_t* out, size_t capacity, size_t& size) {
    if (!guard) return false;
    xSemaphoreTake(guard, portMAX_DELAY);
    const bool have = wallpaperReady && wallpaperSize <= capacity;
    if (have) { memcpy(out, wallpaper, wallpaperSize); size = wallpaperSize; }
    wallpaperReady = false;
    xSemaphoreGive(guard);
    return have;
}
bool navStreamToday(TodayDistance& out) {
    portENTER_CRITICAL(&infoLock); out=today; portEXIT_CRITICAL(&infoLock);
    return out.valid;
}
