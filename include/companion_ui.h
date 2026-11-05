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
