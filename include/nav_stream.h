#pragma once
#include <stddef.h>
#include <stdint.h>
class NimBLEServer;
bool navStreamBegin(NimBLEServer* server);
void navStreamReset();
// Called only from the LVGL/main thread. Published storage stays valid until
// the next take; old LVGL image caches must be dropped before then.
bool navStreamTake(const uint8_t*& bytes, size_t& size);
void navStreamPrintStatus();
bool navStreamExternalGuidance();
int navStreamSpeedTenths(); // -1 when unavailable or older than 5 seconds.
// UI thread -> encrypted ACK/notification channel; frame ID binds the gesture
// to the exact map projection displayed on the board.
bool navStreamControl(uint8_t command, int16_t dx = 0, int16_t dy = 0);

// Trip and weather summaries written by the iPhone (characteristic ee43a106).
struct TripInfo {
    bool active = false;
    uint32_t receivedAt = 0;        // millis(); 0 = never
    uint32_t remainingMetres = 0, turnMetres = 0;
    uint16_t etaMinute = 0xFFFF;     // local minute of day, 0xFFFF = unknown
    uint8_t maneuver = 0;            // 0 straight, 1 left, 2 right, 3 slight left, 4 slight right, 5 U-turn, 6 arrive
    char instruction[152] = {};
};
struct WeatherInfo {
    bool valid = false;
    uint32_t receivedAt = 0;
    uint8_t rainChance = 0;          // max % over the next 3 hours
    uint16_t rainInMinutes = 0xFFFF; // first likely-rain hour, minutes from now
    int16_t tempTenths = 0;
    uint16_t sunriseMinute = 0xFFFF, sunsetMinute = 0xFFFF; // local minute of day
};
// Distance ridden today as measured by the phone's GPS; dayKey = yyyymmdd (phone local date).
struct TodayDistance { bool valid = false; uint32_t metres = 0, dayKey = 0; };
bool navStreamToday(TodayDistance& out);
bool navStreamTrip(TripInfo& out);
bool navStreamWeather(WeatherInfo& out);
// A wallpaper JPEG (320x240) received from the phone, copied into out. UI thread only.
bool navStreamTakeWallpaper(uint8_t* out, size_t capacity, size_t& size);
