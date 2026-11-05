#include "phone_link.h"
#include "nav_stream.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <string>

namespace {
const NimBLEUUID ANCS("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
const NimBLEUUID SOURCE("9FBF120D-6301-42D9-8C58-25E699A21DBD");
const NimBLEUUID CONTROL("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
const NimBLEUUID DATA("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");
const NimBLEUUID AMS("89D3502B-0F36-433A-8EF4-C502AD55F8DC");
const NimBLEUUID MUSIC_REMOTE("9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2");
const NimBLEUUID MUSIC_ENTITY("2F7CABCE-808D-411F-9A0C-BB92BA96C102");
constexpr uint16_t NO_PEER = BLE_HS_CONN_HANDLE_NONE;

enum class InputType : uint8_t { Source, Data, Clock, MusicEntity, MusicCommands };
struct Input {
    InputType type;
    uint32_t session;
    uint16_t size;
    uint8_t data[256];
};
struct SourceInput {
    uint32_t session;
    uint8_t data[8];
};
struct Pending { uint32_t uid; uint8_t category; bool preExisting; bool negative; };
struct DismissRequest { uint32_t session; uint32_t epoch; uint32_t uid; };
struct NameRequest { uint32_t session; char identifier[96]; };
struct MusicRequest { uint32_t session; uint8_t command; };

QueueHandle_t inputs;
QueueHandle_t sources;
QueueHandle_t outputs;
QueueHandle_t musicInputs;
QueueHandle_t musicRequests;
QueueHandle_t dismissRequests;
QueueHandle_t appNameRequests;
NimBLEServer* server;
std::atomic<uint16_t> peer{NO_PEER};
std::atomic<uint32_t> generation{0};
std::atomic<uint32_t> notificationEpoch{0};
std::atomic<bool> servicesChanged{false};
std::atomic<bool> overflow{false};
std::atomic<bool> notificationsReady{false};
constexpr uint32_t NO_REFRESH = UINT32_MAX;
std::atomic<uint32_t> refreshSession{NO_REFRESH};
// Counts only: never log senders, message contents or notification identifiers.
std::atomic<uint32_t> sourceEvents{0}, dataPackets{0}, detailRequests{0};
std::atomic<uint32_t> detailReplies{0}, writeFailures{0}, parseFailures{0}, timeouts{0};
std::atomic<uint32_t> outputDrops{0}, sourceDrops{0};
std::atomic<int> lastWriteError{0};
std::atomic<uint32_t> dismissQueued{0},dismissWrites{0},dismissErrors{0},removedEvents{0};
std::atomic<uint32_t> nameLookups{0},nameReplies{0},nameErrors{0};
std::atomic<bool> musicReady{false};
std::atomic<uint16_t> musicCommands{0};
std::atomic<uint32_t> musicUpdates{0}, musicWrites{0};

void publish(PhoneEvent event, uint32_t session, TickType_t timeout = 0) {
    event.session = session;
    if (session == generation.load() && xQueueSend(outputs, &event, timeout) != pdTRUE) ++outputDrops;
}

void signal(PhoneEventType type, uint32_t session, uint32_t value = 0, TickType_t timeout = 0) {
    PhoneEvent event;
    event.type = type;
    event.value = value;
    publish(event, session, timeout);
}

void receive(InputType type, const uint8_t* data, size_t length) {
    if (type == InputType::MusicEntity || type == InputType::MusicCommands) {
        if (length > sizeof(Input::data)) return;
        Input input{};
        input.type = type;
        input.session = generation.load();
        input.size = length;
        if (length) memcpy(input.data, data, length);
        if (xQueueSend(musicInputs, &input, 0) != pdTRUE) {
            Input oldest;
            xQueueReceive(musicInputs, &oldest, 0);
            xQueueSend(musicInputs, &input, 0);
        }
        return;
    }
    if (type == InputType::Source) {
        if (length != 8) return;
        ++sourceEvents;
        SourceInput source{};
        source.session = generation.load();
        memcpy(source.data, data, 8);
        if (xQueueSend(sources, &source, 0) != pdTRUE) {
            ++sourceDrops;
            // Large initial notification inventories must not block the BLE host
            // or discard fragmented attribute responses. Keep the latest events.
            SourceInput oldest;
            xQueueReceive(sources, &oldest, 0);
            xQueueSend(sources, &source, 0);
        }
        return;
    }
    if (type == InputType::Data) ++dataPackets;
    if (!length || length > sizeof(Input::data)) {
        overflow = true;
        return;
    }
    Input input{};
    input.type = type;
    input.session = generation.load();
    input.size = length;
    memcpy(input.data, data, length);
    if (xQueueSend(inputs, &input, 0) != pdTRUE) overflow = true;
}

class Callbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* instance, NimBLEConnInfo& info) override {
        // A single phone owns the ANCS session and its notification identifiers.
        if (peer.load() != NO_PEER) {
            instance->disconnect(info.getConnHandle());
            return;
        }
        peer = info.getConnHandle();
        navStreamReset();
        const uint32_t session = ++generation;
        signal(PhoneEventType::Connected, session);
        Serial.println("iPhone BLE connected; waiting for pairing");
    }

    void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) override {
        if (info.getConnHandle() != peer.load()) return;
        peer = NO_PEER;
        navStreamReset();
        notificationsReady = false;
        musicReady = false;
        musicCommands = 0;
        signal(PhoneEventType::Disconnected, ++generation);
        Serial.println("iPhone BLE disconnected");
    }

    uint32_t onPassKeyDisplay() override {
        const uint32_t passkey = esp_random() % 1000000;
        signal(PhoneEventType::PairingCode, generation.load(), passkey);
        // The PIN is displayed on the device; notification contents are never logged.
        return passkey;
    }

    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        if (info.getConnHandle() != peer.load()) return;
        if (!info.isEncrypted()) {
            server->disconnect(info.getConnHandle());
            return;
        }
        signal(PhoneEventType::Secured, generation.load());
        // Bonded iPhones may retain a pre-update GATT range. Invalidate the
        // cached database after encryption so newly added channels are read.
        server->sendServiceChangedIndication();
        Serial.println("iPhone BLE pairing/encryption ready");
    }
} callbacks;

// GATT discovery/reads can block. Keep them on a worker so touching the display
// and LVGL rendering stay responsive even while the phone requests permission.
void worker(void*) {
    uint32_t session = UINT32_MAX;
    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* control = nullptr;
    NimBLERemoteCharacteristic* musicRemote = nullptr;
    bool subscribed = false;
    bool changedSubscribed = false;
    bool unavailableReported = false;
    bool clockSubscribed = false;
    bool waiting = false;
    bool canceled = false;
    bool requestedPreExisting = false;
    bool requestedNegative = false;
    bool appRequest = false;
    bool refreshing = false;
    bool refreshFailed = false;
    bool musicSubscribed = false;
    bool musicUnavailableReported = false;
    uint32_t requestedAt = 0;
    uint32_t nextDiscovery = 0;
    uint32_t nextClock = 0;
    uint32_t nextRequest = 0;
    uint32_t nextMusicDiscovery = 0;
    int32_t offset = 7 * 3600;
    phone::AncsParser parser;
    phone::AppNameParser nameParser;
    std::deque<std::string> pendingNames;
    std::deque<std::string> knownNames;
    auto enqueueName=[&](const std::string& identifier) {
        if(identifier.empty() || std::find(knownNames.begin(),knownNames.end(),identifier)!=knownNames.end()) return;
        if(knownNames.size()==32) knownNames.pop_front();
        knownNames.push_back(identifier);
        if(pendingNames.size()<32) pendingNames.push_back(identifier);
    };
    std::deque<Pending> pending;
    for (;;) {
        const uint32_t now = millis();
        const uint16_t handle = peer.load();
        const uint32_t activeSession = generation.load();
        if (session != activeSession) {
            ++notificationEpoch;
            session = activeSession;
            client = nullptr;
            control = nullptr;
            musicRemote = nullptr;
            musicSubscribed = musicUnavailableReported = false;
            musicReady = false;
            musicCommands = 0;
            nextMusicDiscovery = 0;
            Input ignoredMusic;
            while (xQueueReceive(musicInputs, &ignoredMusic, 0) == pdTRUE) {}
            MusicRequest ignoredCommand;
            while (xQueueReceive(musicRequests, &ignoredCommand, 0) == pdTRUE) {}
            subscribed = changedSubscribed = unavailableReported = clockSubscribed = waiting = canceled = false;
            nextDiscovery = nextClock = nextRequest = 0;
            offset = 7 * 3600;
            pending.clear();
            pendingNames.clear(); knownNames.clear(); appRequest=false;
            overflow = false;
            servicesChanged = false;
            notificationsReady = false;
            refreshing = refreshFailed = false;
            auto requested = refreshSession.load();
            if (requested != session) refreshSession.compare_exchange_strong(requested, NO_REFRESH);
            if (handle != NO_PEER) NimBLEDevice::startSecurity(handle);
        }

        if (handle == NO_PEER) {
            Input ignored;
            while (xQueueReceive(inputs, &ignored, 0) == pdTRUE) {}
            SourceInput ignoredSource;
            while (xQueueReceive(sources, &ignoredSource, 0) == pdTRUE) {}
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        const auto info = server->getPeerInfoByHandle(handle);
        if (!info.isEncrypted()) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        if (!client) client = server->getClient(handle);
        if (!client || !client->isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        if (servicesChanged.exchange(false)) {
            ++notificationEpoch;
            // ANCS can be published/unpublished after the iPhone permission changes.
            client->deleteServices();
            control = nullptr;
            musicRemote = nullptr;
            musicSubscribed = false;
            musicReady = false;
            musicCommands = 0;
            nextMusicDiscovery = 0;
            signal(PhoneEventType::MusicUnavailable, session);
            subscribed = changedSubscribed = clockSubscribed = waiting = false;
            notificationsReady = false;
            nextDiscovery = nextClock = 0;
            pending.clear();
            pendingNames.clear(); knownNames.clear(); appRequest=false;
        }

        if (!refreshing && refreshSession.load() == session && !waiting) {
            ++notificationEpoch;
            refreshing = true;
            refreshFailed = false;
            signal(PhoneEventType::RefreshStarted, session, 0, pdMS_TO_TICKS(100));
            Serial.println("ANCS refresh started");
            auto service = client->getService(ANCS);
            auto source = service ? service->getCharacteristic(SOURCE) : nullptr;
            auto data = service ? service->getCharacteristic(DATA) : nullptr;
            // Ending the ANCS subscription invalidates its UIDs. Wait for the
            // current attribute reply before ending it, then discard old input.
            const bool ended = source && source->unsubscribe(true);
            if (ended) {
                if (data) data->unsubscribe(true);
                pending.clear();
                Input ignored;
                while (xQueueReceive(inputs, &ignored, 0) == pdTRUE) {}
                SourceInput ignoredSource;
                while (xQueueReceive(sources, &ignoredSource, 0) == pdTRUE) {}
                overflow = false;
                signal(PhoneEventType::InboxReset, session, 0, pdMS_TO_TICKS(100));
            } else {
                refreshFailed = true;
            }
            // Reinstall callbacks even if unsubscribe failed: NimBLE clears
            // its callback before attempting the CCCD write.
            subscribed = false;
            notificationsReady = false;
            unavailableReported = false;
            nextDiscovery = nextRequest = 0;
        }

        if (!subscribed && static_cast<int32_t>(now - nextDiscovery) >= 0) {
            nextDiscovery = now + 10000;
            if (!changedSubscribed) {
                auto gatt = client->getService(NimBLEUUID(uint16_t(0x1801)));
                auto changed = gatt ? gatt->getCharacteristic(NimBLEUUID(uint16_t(0x2A05))) : nullptr;
                if (changed && changed->canIndicate()) {
                    changedSubscribed = changed->subscribe(false,
                        [](NimBLERemoteCharacteristic*, uint8_t*, size_t, bool) { servicesChanged = true; });
                }
            }
            auto service = client->getService(ANCS);
            auto source = service ? service->getCharacteristic(SOURCE) : nullptr;
            auto data = service ? service->getCharacteristic(DATA) : nullptr;
            control = service ? service->getCharacteristic(CONTROL) : nullptr;
            if (source && data && control && source->canNotify() && data->canNotify() && control->canWrite()) {
                // Apple may deliver Notification Source events immediately upon subscribing.
                // Set up Data Source first and keep all requests serialized in the worker.
                const bool dataReady = data->subscribe(true,
                    [](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
                        receive(InputType::Data, bytes, length);
                    });
                subscribed = dataReady && source->subscribe(true,
                    [](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
                        receive(InputType::Source, bytes, length);
                    });
            }
            if (subscribed) {
                notificationsReady = true;
                signal(PhoneEventType::NotificationsReady, session);
                Serial.println("ANCS subscribed; notification sharing enabled");
            } else if (!unavailableReported) {
                signal(PhoneEventType::NotificationsUnavailable, session);
                unavailableReported = true;
                Serial.println("ANCS not authorized yet; enable Share System Notifications on iPhone");
            }
            if (refreshing) {
                signal(PhoneEventType::RefreshFinished, session,
                       subscribed && !refreshFailed ? 1 : 0, pdMS_TO_TICKS(100));
                Serial.printf("ANCS refresh subscription: %s\n", subscribed && !refreshFailed ? "ready" : "failed");
                uint32_t requested = session;
                refreshSession.compare_exchange_strong(requested, NO_REFRESH);
                refreshing = false;
            }
        }

        if (!musicSubscribed && !waiting && static_cast<int32_t>(now - nextMusicDiscovery) >= 0) {
            nextMusicDiscovery = now + 15000;
            auto media = client->getService(AMS);
            musicRemote = media ? media->getCharacteristic(MUSIC_REMOTE) : nullptr;
            auto entity = media ? media->getCharacteristic(MUSIC_ENTITY) : nullptr;
            if (musicRemote && entity && musicRemote->canWrite() && musicRemote->canNotify() &&
                entity->canWrite() && entity->canNotify()) {
                const bool remoteOk = musicRemote->subscribe(true,
                    [](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
                        receive(InputType::MusicCommands, bytes, length);
                    });
                const bool entityOk = entity->subscribe(true,
                    [](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
                        receive(InputType::MusicEntity, bytes, length);
                    });
                // Register fields after subscribing; initial values can arrive during writes.
                const uint8_t playerFields[] = {0, 0, 1, 2};
                const uint8_t trackFields[] = {2, 0, 2};
                musicSubscribed = remoteOk && entityOk &&
                    entity->writeValue(playerFields, sizeof(playerFields), true) &&
                    entity->writeValue(trackFields, sizeof(trackFields), true);
            }
            if (musicSubscribed) {
                musicReady = true;
                signal(PhoneEventType::MusicReady, session);
                Serial.println("AMS subscribed; iPhone music controls ready");
            } else if (!musicUnavailableReported) {
                musicUnavailableReported = true;
                signal(PhoneEventType::MusicUnavailable, session);
                Serial.println("AMS unavailable; open a music player on iPhone");
            }
        }

        Input mediaInput;
        while (xQueueReceive(musicInputs, &mediaInput, 0) == pdTRUE) {
            if (mediaInput.session != session) continue;
            if (mediaInput.type == InputType::MusicCommands) {
                musicCommands = phone::decodeMusicCommands(mediaInput.data, mediaInput.size);
                signal(PhoneEventType::MusicCommands, session, musicCommands.load());
            } else {
                PhoneEvent event;
                event.type = PhoneEventType::MusicUpdate;
                if (phone::decodeMusicUpdate(mediaInput.data, mediaInput.size, event.music)) {
                    ++musicUpdates;
                    publish(event, session);
                }
            }
        }
        MusicRequest musicRequest;
        if (!waiting && xQueueReceive(musicRequests, &musicRequest, 0) == pdTRUE &&
            musicRequest.session == session && session == generation.load()) {
            if (!musicSubscribed || !musicRemote ||
                !(musicCommands.load() & (uint16_t(1) << musicRequest.command)) ||
                !musicRemote->writeValue(&musicRequest.command, 1, true)) {
                signal(PhoneEventType::MusicCommandFailed, session);
                Serial.println("AMS remote command unavailable or rejected");
            } else {
                ++musicWrites;
                Serial.println("AMS remote command sent");
            }
        }

        if (static_cast<int32_t>(now - nextClock) >= 0 && client->isConnected()) {
            nextClock = now + 15000;
            auto timeService = client->getService(NimBLEUUID(uint16_t(0x1805)));
            auto current = timeService ? timeService->getCharacteristic(NimBLEUUID(uint16_t(0x2A2B))) : nullptr;
            auto local = timeService ? timeService->getCharacteristic(NimBLEUUID(uint16_t(0x2A0F))) : nullptr;
            if (local && local->canRead()) {
                const auto value = local->readValue();
                phone::decodeLocalTime(value.data(), value.length(), offset);
            }
            if (current && current->canRead()) {
                const auto value = current->readValue();
                PhoneEvent event;
                event.type = PhoneEventType::Clock;
                if (phone::decodeCurrentTime(value.data(), value.length(), offset, event.clock)) {
                    publish(event, session);
                    nextClock = now + 15 * 60 * 1000;
                    Serial.println("Clock synchronized from iPhone CTS");
                }
                if (!clockSubscribed && current->canNotify()) {
                    clockSubscribed = current->subscribe(true,
                        [](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
                            receive(InputType::Clock, bytes, length);
                        });
                }
            }
        }

        if (overflow.exchange(false)) {
            waiting = false;
            nextRequest = now + 1000;
            Input ignored;
            while (xQueueReceive(inputs, &ignored, 0) == pdTRUE) {}
            Serial.println("ANCS input overflow; dropping incomplete response");
        }

        SourceInput source;
        while (xQueueReceive(sources, &source, 0) == pdTRUE) {
            if (source.session == session) {
                const uint8_t* p = source.data;
                const uint32_t uid = static_cast<uint32_t>(p[4]) | (static_cast<uint32_t>(p[5]) << 8) |
                    (static_cast<uint32_t>(p[6]) << 16) | (static_cast<uint32_t>(p[7]) << 24);
                pending.erase(std::remove_if(pending.begin(), pending.end(),
                              [uid](const Pending& entry) { return entry.uid == uid; }), pending.end());
                if (p[0] == 2) {
                    ++removedEvents;
                    signal(PhoneEventType::Removed, session, uid);
                    if (waiting && !appRequest && parser.notice().uid == uid) canceled = true;
                } else if (p[0] < 2) {
                    if (pending.size() == 128) pending.pop_front();
                    pending.push_back({uid, p[2], (p[1] & 4) != 0, (p[1] & 16) != 0});
                }
            }
        }

        Input input;
        while (xQueueReceive(inputs, &input, 0) == pdTRUE) {
            if (input.session != session) continue;
            if (input.type == InputType::Data && waiting) {
                if(appRequest) {
                    if(nameParser.feed(input.data,input.size)) {
                        ++nameReplies;
                        if(nameParser.name()[0]) {
                            PhoneEvent event; event.type=PhoneEventType::AppName;
                            strlcpy(event.notice.app,nameParser.identifier(),sizeof(event.notice.app));
                            strlcpy(event.notice.title,nameParser.name(),sizeof(event.notice.title));
                            publish(event,session);
                        }
                        waiting=false; appRequest=false;
                    } else if(nameParser.failed()) { ++nameErrors; waiting=false; appRequest=false; }
                    continue;
                }
                if (parser.feed(input.data, input.size)) {
                    ++detailReplies;
                    if (!canceled) {
                        PhoneEvent event;
                        event.type = PhoneEventType::Notice;
                        event.notice = parser.notice();
                        event.notice.preExisting = requestedPreExisting;
                        event.notice.canDismiss = requestedNegative &&
                            phone::isDismissAction(event.notice.category, event.notice.negativeAction);
                        event.notice.negativeActionAvailable=requestedNegative;
                        event.notice.actionSession=notificationEpoch.load();
                        publish(event, session);
                        enqueueName(event.notice.app);
                        Serial.println("ANCS notification received");
                    }
                    waiting = false;
                } else if (parser.failed()) {
                    ++parseFailures;
                    waiting = false;
                    nextRequest = now + 1000;
                    Serial.println("ANCS attribute response could not be decoded");
                }
            } else if (input.type == InputType::Clock) {
                PhoneEvent event;
                event.type = PhoneEventType::Clock;
                if (phone::decodeCurrentTime(input.data, input.size, offset, event.clock)) publish(event, session);
            }
        }

        if (waiting && now - requestedAt > 5000) {
            if(appRequest) { ++nameErrors; appRequest=false; }
            else ++timeouts;
            waiting = false;
            nextRequest = now + 1000;
            Serial.println("ANCS detail request timed out");
        }
        bool issuedDismiss=false;
        if (!waiting && subscribed && control && refreshSession.load() != session) {
            DismissRequest request;
            if (xQueueReceive(dismissRequests, &request, 0) == pdTRUE && request.session == session && request.epoch==notificationEpoch.load()) {
                issuedDismiss=true;
                const uint32_t uid = request.uid;
                const uint8_t command[] = {2, uint8_t(uid), uint8_t(uid >> 8), uint8_t(uid >> 16), uint8_t(uid >> 24), 1};
                if (!control->writeValue(command, sizeof(command), true)) {
                    ++dismissErrors; signal(PhoneEventType::NoticeActionFailed, session, uid);
                } else ++dismissWrites;
            }
        }
        NameRequest nameRequest;
        while(xQueueReceive(appNameRequests,&nameRequest,0)==pdTRUE)
            if(nameRequest.session==session) enqueueName(nameRequest.identifier);
        if(!issuedDismiss && subscribed && control && client->isConnected() && !waiting && !pendingNames.empty() && refreshSession.load()!=session) {
            const auto identifier=pendingNames.front(); pendingNames.pop_front();
            uint8_t command[100]{}; command[0]=1;
            memcpy(command+1,identifier.c_str(),identifier.size()+1);
            command[identifier.size()+2]=0; // DisplayName attribute.
            nameParser.begin(identifier.c_str()); appRequest=true; waiting=true; requestedAt=now;
            ++nameLookups;
            if(!control->writeValue(command,identifier.size()+3,true)) { ++nameErrors; waiting=false; appRequest=false; }
        }
        if (!issuedDismiss && subscribed && control && client->isConnected() && !waiting && !pending.empty() &&
            refreshSession.load() != session &&
            static_cast<int32_t>(now - nextRequest) >= 0 && session == generation.load()) {
            const Pending entry = pending.front();
            pending.pop_front();
            parser.begin(entry.uid, entry.category, true);
            appRequest=false;
            requestedPreExisting = entry.preExisting;
            requestedNegative = entry.negative;
            canceled = false;
            waiting = true;
            requestedAt = now;
            // Command 0; UID little endian; app ID; title max 159; body max 511.
            const uint8_t command[] = {0, uint8_t(entry.uid), uint8_t(entry.uid >> 8),
                uint8_t(entry.uid >> 16), uint8_t(entry.uid >> 24), 0, 1, 159, 0, 3, 255, 1, 5, 7};
            ++detailRequests;
            if (!control->writeValue(command, sizeof(command), true)) {
                ++writeFailures;
                lastWriteError = client->getLastError();
                waiting = false;
                Serial.printf("ANCS attribute request failed: %d\n", lastWriteError.load());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}

bool phoneLinkBegin() {
    // At the minimum MTU, a full app/title/body reply can span ~48 notifications.
    // Source events have their own compact queue so a phone's initial inventory
    // cannot consume the capacity needed to reassemble a single response.
    inputs = xQueueCreate(64, sizeof(Input));
    sources = xQueueCreate(64, sizeof(SourceInput));
    outputs = xQueueCreate(12, sizeof(PhoneEvent));
    musicInputs = xQueueCreate(16, sizeof(Input));
    musicRequests = xQueueCreate(8, sizeof(MusicRequest));
    dismissRequests = xQueueCreate(16, sizeof(DismissRequest));
    appNameRequests=xQueueCreate(32,sizeof(NameRequest));
    if (!inputs || !sources || !outputs || !musicInputs || !musicRequests || !dismissRequests || !appNameRequests) return false;
    if (!NimBLEDevice::init("HUD iPhone")) return false;
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    server = NimBLEDevice::createServer();
    if (!server) return false;
    server->setCallbacks(&callbacks, false);
    server->advertiseOnDisconnect(true);

    auto information = server->createService(NimBLEUUID(uint16_t(0x180A)));
    information->createCharacteristic(NimBLEUUID(uint16_t(0x2A24)), NIMBLE_PROPERTY::READ)->setValue("ES3C28P HUD");
    information->createCharacteristic(NimBLEUUID(uint16_t(0x2A29)), NIMBLE_PROPERTY::READ)->setValue("DIY HUD");
    if (!navStreamBegin(server)) return false;
    if (!server->start()) return false;

    NimBLEAdvertisementData advertisement;
    advertisement.setFlags(0x06);
    advertisement.setAppearance(0x0140);  // Generic display (Bluetooth appearance category 5).
    // AD type 0x15: 128-bit service solicitation, not a service we provide.
    const uint8_t solicit[] = {0x11, 0x15, 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B,
        0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79};
    advertisement.addData(solicit, sizeof(solicit));
    advertisement.addServiceUUID(NimBLEUUID(uint16_t(0x180A)));
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName("HUD iPhone");
    const uint8_t solicitMusic[] = {0x11, 0x15, 0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5,
        0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};
    scanResponse.addData(solicitMusic, sizeof(solicitMusic));
    auto advertising = NimBLEDevice::getAdvertising();
    advertising->setAdvertisementData(advertisement);
    advertising->setScanResponseData(scanResponse);
    advertising->enableScanResponse(true);
    if (xTaskCreate(worker, "iphone-gatt", 8192, nullptr, 1, nullptr) != pdPASS) return false;
    if (!advertising->start()) return false;
    Serial.println("HUD iPhone advertising; pair in iPhone Settings > Bluetooth");
    return true;
}

bool phoneLinkPoll(PhoneEvent& event) {
    while (outputs && xQueueReceive(outputs, &event, 0) == pdTRUE) {
        if (event.session == generation.load()) return true;
    }
    return false;
}

PhoneRefreshResult phoneLinkRefreshNotifications() {
    const uint32_t session = generation.load();
    const uint16_t handle = peer.load();
    if (handle == NO_PEER || !server) return PhoneRefreshResult::Disconnected;
    if (!server->getPeerInfoByHandle(handle).isEncrypted()) return PhoneRefreshResult::Pairing;
    uint32_t idle = NO_REFRESH;
    if (!refreshSession.compare_exchange_strong(idle, session)) return PhoneRefreshResult::Busy;
    return PhoneRefreshResult::Queued;
}

bool phoneLinkMusicCommand(PhoneMusicCommand command) {
    const uint8_t id = static_cast<uint8_t>(command);
    if (id > 6 || !musicReady.load() || peer.load() == NO_PEER ||
        !(musicCommands.load() & (uint16_t(1) << id)) || !musicRequests) return false;
    MusicRequest request{generation.load(), id};
    return xQueueSend(musicRequests, &request, 0) == pdTRUE;
}

bool phoneLinkDismiss(const phone::Notice& notice) {
    if (!notice.canDismiss || !phone::isDismissAction(notice.category, notice.negativeAction) ||
        !notificationsReady.load() || !dismissRequests || refreshSession.load()!=NO_REFRESH || servicesChanged.load() ||
        notice.actionSession!=notificationEpoch.load()) return false;
    const DismissRequest request{generation.load(), notificationEpoch.load(), notice.uid};
    if(xQueueSend(dismissRequests, &request, 0)!=pdTRUE) return false;
    ++dismissQueued; return true;
}

void phoneLinkRequestAppName(const char* identifier) {
    if(!appNameRequests || !notificationsReady.load() || !identifier[0] || strlen(identifier)>=96) return;
    NameRequest request{}; request.session=generation.load(); strlcpy(request.identifier,identifier,sizeof(request.identifier));
    xQueueSend(appNameRequests,&request,0);
}

void phoneLinkPrintStatus() {
    const uint16_t handle = peer.load();
    const bool encrypted = handle != NO_PEER && server &&
                           server->getPeerInfoByHandle(handle).isEncrypted();
    Serial.printf("iPhone link: connected=%u; encrypted=%u; ANCS=%u; session=%u\n",
                  handle != NO_PEER, encrypted, notificationsReady.load(), generation.load());
    Serial.printf("ANCS counts: source=%u; data=%u; requests=%u; replies=%u; writeErrors=%u; parseErrors=%u; timeouts=%u; sourceDrops=%u; outputDrops=%u; lastWriteError=%d\n",
                  sourceEvents.load(), dataPackets.load(), detailRequests.load(), detailReplies.load(),
                  writeFailures.load(), parseFailures.load(), timeouts.load(), sourceDrops.load(),
                  outputDrops.load(), lastWriteError.load());
    Serial.printf("AMS: ready=%u; commands=0x%04x; updates=%u; writes=%u\n",
                  musicReady.load(), musicCommands.load(), musicUpdates.load(), musicWrites.load());
    Serial.printf("Dismiss counts: queued=%u; writes=%u; errors=%u; removals=%u\n",
        dismissQueued.load(),dismissWrites.load(),dismissErrors.load(),removedEvents.load());
    Serial.printf("App name counts: requests=%u; replies=%u; errors=%u\n",nameLookups.load(),nameReplies.load(),nameErrors.load());
    navStreamPrintStatus();
}
