#include "battery.h"

#include <Arduino.h>

namespace {
constexpr int PIN_BAT_ADC = 9;
// The vendor does not publish the divider values. 2.0 (equal resistors) is the
// usual choice; if the printed voltage differs from a multimeter reading, adjust.
constexpr float DIVIDER_RATIO = 2.0f;
constexpr uint32_t SAMPLE_MS = 2000;
constexpr uint32_t TREND_MS = 60000;
constexpr int NO_BATTERY_MV = 2800;
constexpr int RISE_MV = 8;   // rise over TREND_MS => charging
constexpr int FALL_MV = 4;   // fall over TREND_MS => discharging
constexpr int FULL_MV = 4200; // at/above this, USB charger is almost surely attached
// Measured: plugging USB lifts the sensed voltage ~230 mV at once (charger current
// through the pack), unplugging drops it back. A step this large flags the change.
constexpr int STEP_MV = 120;
constexpr int CHARGE_LIFT_MV = 230;

struct Point { int mv; int pct; };
constexpr Point CURVE[] = {{3300,0},{3500,5},{3600,10},{3700,25},{3750,40},
                           {3800,55},{3900,70},{4000,82},{4100,93},{4200,100}};

BatteryStatus status;
float smoothed = 0;
uint32_t lastSample = 0;
int trendRef = 0;
int restPercent = -1;
float chargeEst = -1;
constexpr float CAPACITY_MAH = 1000.0f;
constexpr float CHARGE_MA = 290.0f;   // vendor "actual" charge current
uint32_t changedAt = 0;
constexpr uint32_t HOLD_MS = 15000;   // min time between charging flips
constexpr uint32_t SETTLE_MS = 45000; // hold % after unplug while voltage relaxes
constexpr int HIST = 12;      // raw samples, 2 s apart => ~24 s window
int history[HIST] = {};      // newest last
constexpr int FALL_WINDOW_MV = 25;
uint32_t trendRefAt = 0;

int percentFor(int mv) {
    constexpr int n = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv <= CURVE[0].mv) return 0;
    if (mv >= CURVE[n - 1].mv) return 100;
    for (int i = 1; i < n; ++i) {
        if (mv <= CURVE[i].mv) {
            return CURVE[i - 1].pct + (CURVE[i].pct - CURVE[i - 1].pct) * (mv - CURVE[i - 1].mv) /
                                          (CURVE[i].mv - CURVE[i - 1].mv);
        }
    }
    return 100;
}

int readMillivolts() {
    uint32_t sum = 0;
    for (int i = 0; i < 16; ++i) sum += analogReadMilliVolts(PIN_BAT_ADC);
    return int(sum / 16 * DIVIDER_RATIO);
}
}  // namespace

void batteryBegin() {
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
    lastSample = millis() - SAMPLE_MS;
}

void setCharging(bool on, uint32_t nowMs) {
    if (status.charging == on) return;
    if (nowMs - changedAt < HOLD_MS && changedAt != 0) return;  // ignore flicker
    status.charging = on;
    chargeEst = -1;
    changedAt = nowMs;
    smoothed = 0;
    trendRef = 0;
}

void batteryTick(uint32_t nowMs) {
    if (nowMs - lastSample < SAMPLE_MS) return;
    lastSample = nowMs;
    const int mv = readMillivolts();
    if (mv < NO_BATTERY_MV) {
        status = BatteryStatus{}; smoothed = 0; trendRef = 0; restPercent = -1;
        for (int& h : history) h = 0;
        return;
    }
    const int old3 = history[HIST - 3];   // ~6 s ago
    const int oldest = history[0];        // ~24 s ago
    for (int i = 0; i < HIST - 1; ++i) history[i] = history[i + 1];
    history[HIST - 1] = mv;
    if (old3 > 0 && mv - old3 >= STEP_MV) setCharging(true, nowMs);
    else if (oldest > 0 && oldest - mv >= FALL_WINDOW_MV) setCharging(false, nowMs);

    smoothed = smoothed == 0 ? float(mv) : smoothed * 0.8f + mv * 0.2f;
    const int v = int(smoothed);
    if (trendRef == 0) { trendRef = v; trendRefAt = nowMs; }
    if (nowMs - trendRefAt >= TREND_MS) {
        if (v - trendRef >= RISE_MV) setCharging(true, nowMs);
        trendRef = v; trendRefAt = nowMs;
    }
    if (v >= FULL_MV) setCharging(true, nowMs);

    status.present = true;
    status.millivolts = v;
    if (status.charging) {
        // The sensed voltage is inflated by the charger and wobbles with charge
        // current, so do not derive a percentage from it. Keep the last resting
        // value; with none (booted on USB), estimate once and then only allow it
        // to rise slowly.
        if (restPercent < 0) restPercent = percentFor(v - CHARGE_LIFT_MV);
        if (chargeEst < 0) chargeEst = restPercent;
        // Coulomb-count the charger: 290 mA into 1000 mAh is ~0.0081 %/s.
        chargeEst += CHARGE_MA / CAPACITY_MAH * 100.0f / 3600.0f * (SAMPLE_MS / 1000.0f);
        // Constant-voltage taper: do not claim full until the sensed voltage says so.
        float cap = v >= FULL_MV ? 100.0f : v >= 4150 ? 90.0f + (v - 4150) * 0.2f : 90.0f;
        if (chargeEst > cap) chargeEst = cap;
        if (v >= FULL_MV) chargeEst = 100.0f;
        restPercent = int(chargeEst);
        status.percent = restPercent;
    } else {
        // Right after unplugging the voltage is still relaxing: hold the old value.
        if (restPercent < 0 || nowMs - changedAt >= SETTLE_MS || changedAt == 0)
            restPercent = percentFor(v);
        status.percent = restPercent;
    }
}

BatteryStatus batteryRead() { return status; }
