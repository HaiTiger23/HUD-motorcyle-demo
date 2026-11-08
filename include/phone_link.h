#pragma once

#include <phone_protocol.h>

enum class PhoneEventType : uint8_t {
    Connected, Disconnected, Secured, NotificationsReady, NotificationsUnavailable,
    PairingCode, Clock, Notice, Removed,
    RefreshStarted, InboxReset, RefreshFinished,
    MusicReady, MusicUnavailable, MusicUpdate, MusicCommands, MusicCommandFailed,
    NoticeActionFailed, AppName
};

struct PhoneEvent {
    PhoneEventType type = PhoneEventType::Disconnected;
    uint32_t session = 0;
    uint32_t value = 0;
    phone::ClockSample clock;
    phone::Notice notice;
    phone::MusicUpdate music;
};

// Bluetooth callbacks only enqueue data. LVGL is updated on the main loop.
bool phoneLinkBegin();
bool phoneLinkPoll(PhoneEvent& event);
enum class PhoneRefreshResult : uint8_t { Queued, Disconnected, Pairing, Busy };
PhoneRefreshResult phoneLinkRefreshNotifications();
enum class PhoneMusicCommand : uint8_t { Play = 0, Pause = 1, Toggle = 2, Next = 3, Previous = 4, VolumeUp = 5, VolumeDown = 6 };
bool phoneLinkMusicCommand(PhoneMusicCommand command);
void phoneLinkPrintStatus();
// Quick-settings BLE switch: false disconnects the iPhone and stops advertising.
void phoneLinkSetEnabled(bool enabled);
bool phoneLinkEnabled();
bool phoneLinkDismiss(const phone::Notice& notice);
void phoneLinkRequestAppName(const char* identifier);
