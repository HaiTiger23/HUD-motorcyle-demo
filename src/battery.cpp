#include "battery.h"
#include "board_config.h"

#include <Arduino.h>
#include <algorithm>
#include <cstdlib>
#include <nvs.h>

namespace {
constexpr uint32_t SAMPLE_MS = 2000;
constexpr int NO_BATTERY_MV = 2800;
constexpr int CHARGE_STEP_MV = 120;
constexpr int CHARGE_CONFIRM_MV = 100;
constexpr uint32_t CHARGE_TREND_MS = 45000;
constexpr int USB_CHARGE_RISE_MV = 15;
constexpr int CHARGE_RISE_MV = 25;
constexpr uint32_t CHARGE_EVIDENCE_MS = 90000;
constexpr int MIN_CALIBRATION_LIFT_MV = 60;
constexpr int MAX_CHARGE_LIFT_MV = 300;
constexpr int DEFAULT_CHARGE_LIFT_MV = 230; // ~3910 mV on USB and ~3680 mV at the reported settled 22%
constexpr int CHARGE_TAPER_MV = 4050;
constexpr int FULL_READING_MV = 4175; // measured by GPIO9 after the user reported a full charge
constexpr int FULL_CANDIDATE_MV = FULL_READING_MV - 5;
constexpr int FULL_HOLD_MV = FULL_READING_MV - 10;
constexpr int FULL_SPREAD_MV = 8;
constexpr uint32_t FULL_STABLE_MS = 60000;
constexpr uint32_t UNPLUG_SETTLE_MS = 6000;
constexpr uint32_t UNPLUG_TIMEOUT_MS = 16000;
constexpr int UNPLUG_STABLE_MV = 12;
constexpr uint32_t UNPLUG_CALIBRATION_MS = 42000;
constexpr uint32_t UNPLUG_CALIBRATION_TIMEOUT_MS = 90000;
constexpr int CALIBRATION_SAMPLES = 6;
constexpr int CALIBRATION_SPREAD_MV = 10;
constexpr uint32_t BACKLIGHT_SETTLE_MS = 6000;
constexpr int BACKLIGHT_CHANGE = 12;
constexpr uint32_t PERCENT_CONFIRM_MS = 8000;
constexpr uint32_t INITIAL_LOAD_MS = 4000;
constexpr uint32_t INITIAL_WAIT_MS = 20000;
constexpr int INITIAL_SAMPLE_COUNT = 4;
constexpr int INITIAL_SPREAD_MV = 16;
constexpr int MIN_SAVED_CHANGE_MV = 10;
constexpr char SETTINGS_NAMESPACE[] = "hud-settings";
constexpr char CHARGE_LIFT_KEY[] = "bat-lift-load"; // calibration after voltage settles under load

struct Point { int mv; int pct; };
constexpr Point CURVE[] = {{3300,0},{3500,5},{3600,10},{3700,25},{3750,40},
                           {3800,55},{3900,70},{4000,82},{4100,93},{FULL_READING_MV,100}};

BatteryStatus status;
uint32_t lastSample = 0;
float smoothedMv = 0, smoothedEstimateMv = 0;
int previousMv = 0;
int stepBaseMv = 0;
bool voltageStepPowered = false;
int baselineLiftMv = DEFAULT_CHARGE_LIFT_MV;
int savedLiftMv = 0;
int chargeLiftMv = DEFAULT_CHARGE_LIFT_MV;
int chargeTaperMv = CHARGE_TAPER_MV;
bool unplugSettling = false;
int unpluggedFromMv = 0;
uint32_t unpluggedAt = 0;
int unpluggedDuty = 0;
int calibrationCount = 0, calibrationSum = 0, calibrationMin = 0, calibrationMax = 0;
int backlightReference = -1;
uint32_t backlightChangedAt = 0;
int percentDirection = 0;
uint32_t percentDirectionAt = 0;
uint32_t initialLevelAt = 0;
int initialSamples[INITIAL_SAMPLE_COUNT];
int initialSampleCount = 0;
bool initialPowered = false;
bool fullEstimated = false;
int fullMinMv = 0, fullMaxMv = 0;
uint32_t fullSince = 0;
int trendBaseMv = 0;
uint32_t trendAt = 0;
uint32_t lastChargeEvidence = 0;

int percentFor(int mv) {
    if (mv <= CURVE[0].mv) return 0;
    for (unsigned i = 1; i < sizeof(CURVE) / sizeof(CURVE[0]); ++i) {
        const Point& low = CURVE[i - 1];
        const Point& high = CURVE[i];
        if (mv <= high.mv) {
            const int span = high.mv - low.mv;
            return low.pct + ((high.pct - low.pct) * (mv - low.mv) + span / 2) / span;
        }
    }
    return 100;
}

int readMillivolts() {
    constexpr int samples = 16;
    uint32_t sum = 0;
    for (int i = 0; i < samples; ++i) sum += analogReadMilliVolts(board::BAT_ADC);
    // R14 and R15 are both 200 kΩ on the ES3C28P / ES3N28P schematic.
    return int((sum * 2 + samples / 2) / samples);
}

bool usbHostPresent() {
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
    return HWCDC::isPlugged(); // USB host traffic, unavailable with power-only adapters.
#else
    return false;
#endif
}

void setChargeLift(int lift, int measuredAt) {
    chargeLiftMv = lift;
    chargeTaperMv = std::max(CHARGE_TAPER_MV, std::min(measuredAt, FULL_READING_MV - 1));
}

bool validCalibrationLift(int lift) {
    return lift >= MIN_CALIBRATION_LIFT_MV && lift <= MAX_CHARGE_LIFT_MV;
}

void saveChargeLift(int lift) {
    baselineLiftMv = lift;
    if (std::abs(lift - savedLiftMv) < MIN_SAVED_CHANGE_MV) return;
    nvs_handle_t handle;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    if (nvs_set_u16(handle, CHARGE_LIFT_KEY, lift) == ESP_OK && nvs_commit(handle) == ESP_OK) savedLiftMv = lift;
    nvs_close(handle);
}

void sampleUnplugCalibration(int mv, bool loadStable, uint32_t elapsed) {
    if (!unpluggedFromMv) return;
    if (elapsed >= UNPLUG_CALIBRATION_TIMEOUT_MS) {
        unpluggedFromMv = 0;
        return;
    }
    if (elapsed < UNPLUG_CALIBRATION_MS || !loadStable ||
        std::abs(backlightReference - unpluggedDuty) >= BACKLIGHT_CHANGE || backlightReference == 0) {
        calibrationCount = 0;
        return;
    }

    if (calibrationCount && std::max(calibrationMax, mv) - std::min(calibrationMin, mv) > CALIBRATION_SPREAD_MV)
        calibrationCount = 0;
    if (calibrationCount == 0) {
        calibrationMin = calibrationMax = mv;
        calibrationSum = 0;
    }
    calibrationMin = std::min(calibrationMin, mv);
    calibrationMax = std::max(calibrationMax, mv);
    calibrationSum += mv;
    if (++calibrationCount < CALIBRATION_SAMPLES) return;
    const int lift = unpluggedFromMv - (calibrationSum + CALIBRATION_SAMPLES / 2) / CALIBRATION_SAMPLES;
    unpluggedFromMv = 0;
    if (validCalibrationLift(lift)) saveChargeLift(lift);
}

void updateUnplugTransition(int mv, bool usbHost, bool unplugged, bool loadStable, int change, uint32_t now) {
    if (unplugged) {
        unplugSettling = true;
        // A near-full cell is already in the region where compensation tapers.
        unpluggedFromMv = status.usbHost && loadStable && backlightReference > 0 &&
                          previousMv <= CHARGE_TAPER_MV ? previousMv : 0;
        unpluggedAt = now;
        unpluggedDuty = backlightReference;
        calibrationCount = 0;
    }
    if (usbHost || voltageStepPowered || stepBaseMv) {
        unplugSettling = false;
        unpluggedFromMv = 0;
        return;
    }
    const uint32_t elapsed = now - unpluggedAt;
    if (elapsed >= UNPLUG_TIMEOUT_MS ||
        (elapsed >= UNPLUG_SETTLE_MS && std::abs(change) <= UNPLUG_STABLE_MV &&
         (!unpluggedFromMv || unpluggedFromMv - mv >= MIN_CALIBRATION_LIFT_MV)))
        unplugSettling = false;
    sampleUnplugCalibration(mv, loadStable, elapsed);
}

void updateChargingEstimate(int mv, bool usbHost, bool loadStable, uint32_t now) {
    const int change = previousMv ? mv - previousMv : 0;
    const int stepLift = stepBaseMv ? mv - stepBaseMv : 0;
    const bool confirmedStep = loadStable && stepLift >= CHARGE_CONFIRM_MV && stepLift <= MAX_CHARGE_LIFT_MV;
    const bool unplugged = !usbHost && (status.usbHost || (voltageStepPowered && change <= -CHARGE_CONFIRM_MV));
    const bool dropped = unplugged || change <= -CHARGE_CONFIRM_MV;
    const bool newStep = loadStable && !status.usbHost && !voltageStepPowered &&
                         change >= CHARGE_STEP_MV && change <= MAX_CHARGE_LIFT_MV;
    stepBaseMv = newStep && !confirmedStep ? previousMv : 0;
    if (unplugged) {
        voltageStepPowered = false;
        setChargeLift(baselineLiftMv, CHARGE_TAPER_MV);
    } else if (confirmedStep) {
        voltageStepPowered = !usbHost;
        setChargeLift(stepLift, mv);
    } else if (loadStable && usbHost && !status.usbHost && !voltageStepPowered &&
               change >= 45 && change <= MAX_CHARGE_LIFT_MV) {
        setChargeLift(change, mv);
    }
    updateUnplugTransition(mv, usbHost, unplugged, loadStable, change, now);
    previousMv = mv;

    if (dropped) {
        status.chargingEstimated = false;
        stepBaseMv = 0;
    } else if (confirmedStep) {
        status.chargingEstimated = true;
        lastChargeEvidence = now;
    }

    if (!loadStable || !trendBaseMv || dropped || confirmedStep || usbHost != status.usbHost) {
        trendBaseMv = mv;
        trendAt = now;
    } else if (now - trendAt >= CHARGE_TREND_MS) {
        // A weak rise alone is ambiguous under load. Use it with a USB host
        // or to extend an estimate that started with a confirmed voltage step.
        const int riseThreshold = usbHost ? USB_CHARGE_RISE_MV : CHARGE_RISE_MV;
        if ((usbHost || status.chargingEstimated) && mv - trendBaseMv >= riseThreshold) {
            status.chargingEstimated = true;
            lastChargeEvidence = now;
        }
        trendBaseMv = mv;
        trendAt = now;
    }
    if (now - lastChargeEvidence >= CHARGE_EVIDENCE_MS) status.chargingEstimated = false;
}

int gaugeMillivolts(int mv, bool usbHost) {
    // Correct the observed lift before smoothing so plug/unplug samples are on
    // the same voltage scale. Charge detection may expire while still plugged.
    if (stepBaseMv) return stepBaseMv;
    if ((!usbHost && !voltageStepPowered) || mv >= FULL_READING_MV) return mv;
    const int headroom = std::min(FULL_READING_MV - chargeTaperMv, FULL_READING_MV - mv);
    return mv - chargeLiftMv * headroom / (FULL_READING_MV - chargeTaperMv);
}

void updateFullEstimate(int mv, bool powered, uint32_t now) {
    if (fullEstimated && powered && mv >= FULL_HOLD_MV) return;
    fullEstimated = false;
    if (!powered || mv < FULL_CANDIDATE_MV) {
        fullMinMv = 0;
        return;
    }
    const int low = std::min(fullMinMv, mv);
    const int high = std::max(fullMaxMv, mv);
    if (!fullMinMv || high - low > FULL_SPREAD_MV) {
        fullMinMv = fullMaxMv = mv;
        fullSince = now;
        return;
    }
    fullMinMv = low;
    fullMaxMv = high;
    // An ADC plateau can estimate a full level. The charger has no status GPIO.
    fullEstimated = now - fullSince >= FULL_STABLE_MS;
}

int displayPercentFor(int mv, bool powered) {
    const int percent = percentFor(mv);
    if (!powered) return percent;
    return fullEstimated ? 100 : std::min(99, percent);
}

bool backlightSettled(uint8_t duty, uint32_t now) {
    if (backlightReference < 0) {
        backlightReference = duty;
        backlightChangedAt = now - BACKLIGHT_SETTLE_MS;
    } else if ((duty == 0) != (backlightReference == 0) || std::abs(duty - backlightReference) >= BACKLIGHT_CHANGE) {
        backlightReference = duty;
        backlightChangedAt = now;
    }
    return now - backlightChangedAt >= BACKLIGHT_SETTLE_MS;
}

void initializePercent(int mv, bool powered, bool useSample, uint32_t now) {
    if (!useSample || powered != initialPowered) initialSampleCount = 0;
    initialPowered = powered;
    const uint32_t elapsed = now - initialLevelAt;
    if (!useSample || elapsed < INITIAL_LOAD_MS) return;
    if (initialSampleCount == INITIAL_SAMPLE_COUNT) {
        for (int i = 1; i < INITIAL_SAMPLE_COUNT; ++i) initialSamples[i - 1] = initialSamples[i];
        --initialSampleCount;
    }
    initialSamples[initialSampleCount++] = mv;
    if (initialSampleCount < INITIAL_SAMPLE_COUNT) return;
    const auto extremes = std::minmax_element(initialSamples, initialSamples + INITIAL_SAMPLE_COUNT);
    if (*extremes.second - *extremes.first > INITIAL_SPREAD_MV && elapsed < INITIAL_WAIT_MS) return;

    // Average the two middle readings so a startup spike cannot set the gauge.
    int sum = 0;
    for (int sample : initialSamples) sum += sample;
    const int startMv = (sum - *extremes.first - *extremes.second + 1) / 2;
    smoothedEstimateMv = startMv;
    status.percent = displayPercentFor(startMv, powered);
    percentDirection = 0;
}

void updatePercent(int mv, bool powered, bool useSample, uint32_t now) {
    if (status.percent < 0) {
        initializePercent(mv, powered, useSample, now);
        return;
    }
    if (!useSample) {
        percentDirection = 0;
        return;
    }

    smoothedEstimateMv = smoothedEstimateMv * 0.8f + mv * 0.2f;
    const int target = displayPercentFor(int(smoothedEstimateMv + 0.5f), powered);
    const int measured = displayPercentFor(mv, powered);
    // Battery voltage can rebound when the display sleeps. Capacity can only
    // increase with external power. A short sag can outlast the raw reading in
    // the filter, so require both readings to agree on the direction.
    int direction = 0;
    if (target < status.percent && measured < status.percent) direction = -1;
    if (powered && target > status.percent && measured > status.percent) direction = 1;
    if (direction != percentDirection) {
        percentDirection = direction;
        percentDirectionAt = now;
    } else if (direction && now - percentDirectionAt >= PERCENT_CONFIRM_MS) {
        status.percent += direction;
    }
}
}  // namespace

void batteryBegin() {
    analogSetPinAttenuation(board::BAT_ADC, ADC_11db);
    nvs_handle_t handle;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint16_t stored = 0;
        if (nvs_get_u16(handle, CHARGE_LIFT_KEY, &stored) == ESP_OK && validCalibrationLift(stored)) {
            savedLiftMv = baselineLiftMv = stored;
        }
        nvs_close(handle);
    }
    setChargeLift(baselineLiftMv, CHARGE_TAPER_MV);
    initialLevelAt = millis();
    lastSample = initialLevelAt - SAMPLE_MS;
}

void batteryTick(uint32_t nowMs, uint8_t backlightDuty) {
    if (nowMs - lastSample < SAMPLE_MS) return;
    lastSample = nowMs;
    const int mv = readMillivolts();
    const bool usbHost = usbHostPresent();
    const bool loadStable = backlightSettled(backlightDuty, nowMs);
    if (mv < NO_BATTERY_MV) {
        status = BatteryStatus{};
        status.usbHost = usbHost;
        smoothedMv = smoothedEstimateMv = 0;
        previousMv = stepBaseMv = trendBaseMv = 0;
        voltageStepPowered = false;
        unplugSettling = false;
        unpluggedFromMv = 0;
        initialSampleCount = 0;
        initialLevelAt = nowMs;
        updateFullEstimate(0, false, nowMs);
        setChargeLift(baselineLiftMv, CHARGE_TAPER_MV);
        return;
    }

    updateChargingEstimate(mv, usbHost, loadStable, nowMs);
    const int estimateMv = gaugeMillivolts(mv, usbHost);
    const bool powered = usbHost || voltageStepPowered;
    smoothedMv = smoothedMv == 0 ? float(mv) : smoothedMv * 0.8f + mv * 0.2f;
    const int measuredMv = int(smoothedMv + 0.5f);
    updateFullEstimate(measuredMv, powered, nowMs);
    const bool useSample = !unplugSettling && !stepBaseMv && loadStable && (powered || backlightDuty != 0);
    updatePercent(estimateMv, powered, useSample, nowMs);
    status.present = true;
    status.usbHost = usbHost;
    status.millivolts = measuredMv;
    status.estimateMillivolts = int(smoothedEstimateMv + 0.5f);
}

BatteryStatus batteryRead() { return status; }
