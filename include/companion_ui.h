#pragma once

#include "phone_link.h"

void companionUiBegin();
void companionUiHandle(const PhoneEvent& event);
void companionUiTick();
void companionUiSetTime(const phone::ClockSample& sample, bool fromPhone);
void companionUiBluetoothFailed();
void companionUiPrintStatus();
bool companionUiNavigationActive();
bool companionUiTouch(bool down,int x,int y,bool wakeOnly);
void companionUiLayout(int width,int height);
// True once after a new (not pre-existing) allowed notification arrived; wakes the display.
bool companionUiConsumeWakeRequest();
// Backlight level in percent: the manual setting, or the day/night level when automatic.
uint8_t companionUiBrightness();
