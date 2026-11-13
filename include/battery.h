#pragma once

#include <stdint.h>

// Battery gauge for the ES3C28P / ES3N28P: the LiPo voltage is divided and read
// on GPIO9 (vendor pin table: "Battery voltage ADC"). The TP4054 charger has no
// status pin wired to the ESP32. USB host detection and voltage trends are separate.
struct BatteryStatus {
    bool present = false;   // false when GPIO9 reads like an unconnected battery
    bool usbHost = false;   // USB data connection, independent of battery current
    bool chargingEstimated = false;
    int percent = -1;       // -1 while measuring, otherwise 0..100
    int millivolts = 0;     // battery voltage after the divider correction
    int estimateMillivolts = 0; // voltage used by the percentage curve after charge compensation
};

void batteryBegin();
void batteryTick(uint32_t nowMs, uint8_t backlightDuty);  // samples at most every 2 s
BatteryStatus batteryRead();
