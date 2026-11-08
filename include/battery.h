#pragma once

#include <stdint.h>

// Battery gauge for the ES3C28P / ES3N28P: the LiPo voltage is divided and read
// on GPIO9 (vendor pin table: "Battery voltage ADC"). The TP4054 charger has no
// documented status GPIO, so charging is inferred from the voltage trend.
struct BatteryStatus {
    bool present = false;   // false when GPIO9 reads like an unconnected battery
    bool charging = false;  // inferred, see battery.cpp
    int percent = 0;        // 0..100
    int millivolts = 0;     // battery voltage after the divider correction
};

void batteryBegin();
void batteryTick(uint32_t nowMs);  // cheap; samples at most every 2 s
BatteryStatus batteryRead();
