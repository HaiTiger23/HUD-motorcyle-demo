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
