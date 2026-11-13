#pragma once

#include "phone_link.h"

void companionUiBegin();
void companionUiHandle(const PhoneEvent& event);
void companionUiTick();
void companionUiSetTime(const phone::ClockSample& sample, bool fromPhone);
void companionUiBluetoothFailed();
void companionUiPrintStatus();
bool companionUiNavigationActive();
// touches/x2/y2: second contact for two-finger map gestures.
bool companionUiTouch(bool down,int x,int y,bool wakeOnly,int touches=1,int x2=0,int y2=0);
void companionUiLayout(int width,int height);
// True once after a new (not pre-existing) allowed notification arrived; wakes the display.
bool companionUiConsumeWakeRequest();
// Backlight level in percent: the manual setting, or the day/night level when automatic.
uint8_t companionUiBrightness();
// Serial "JPEGBENCH": decode a 320x240 JPEG like a map frame and report the time.
void companionUiJpegBench();
