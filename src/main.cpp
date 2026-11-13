#include <Arduino.h>
#include <algorithm>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <nvs.h>
#include <lvgl.h>

#include "board_config.h"
#include "ai_voice.h"
#include "companion_ui.h"
#include "battery.h"
#include "wallpaper.h"
#include "lcd_dma.h"
#include "ili9341v_panel.h"
#include "phone_link.h"
#include "hud_settings.h"
#include "ui.h"
#include <display_geometry.h>

namespace {
constexpr uint16_t SCREEN_WIDTH = 320;
constexpr uint16_t SCREEN_HEIGHT = 240;
constexpr uint8_t TFT_ROTATION = 1;
constexpr uint16_t LVGL_BUFFER_LINES = 40; // fewer strips per frame (25.6 KB internal RAM)
constexpr uint32_t HUD_UPDATE_INTERVAL_MS = 700;
constexpr uint32_t BUTTON_PULSE_MS = 180;

TFT_eSPI tft;
lv_display_t* display;
alignas(4) uint16_t displayBuffer[SCREEN_WIDTH * LVGL_BUFFER_LINES];
// Second strip for esp_lcd DMA: LVGL renders one while the other is being sent.
alignas(4) uint16_t displayBuffer2[SCREEN_WIDTH * LVGL_BUFFER_LINES];
bool displayDma = false;
constexpr uint8_t MADCTL[4] = {0x48, 0x28, 0x88, 0xE8}; // TFT_eSPI ILI9341 rotations, BGR
uint32_t lastHudUpdate = 0;
uint32_t buttonPressedUntil = 0;
lv_obj_t* pressedButton = nullptr;
lv_indev_t* touchInput = nullptr;
uint32_t lastTouchAt = 0;
bool screenAwake = true;
bool swallowWakeTouch = false;
bool lastTouchDown=false;
uint8_t displayRotation=0;
enum class PanelPower { Awake, Asleep, Waking };
PanelPower panelPower = PanelPower::Awake;
uint32_t panelCommandAt = 0;
uint32_t activeCpuMhz = 0;
constexpr uint32_t PANEL_SLEEP_DELAY_MS = 120;
constexpr uint32_t IDLE_CPU_MHZ = 80;

void setBacklight(bool awake) {
    static int previous=-1;
#ifdef HUD_DEMO_ONLY
    const int duty=huddisplay::duty(hudHardwareSettingsGet().brightness,awake);
#else
    const int duty=huddisplay::duty(companionUiBrightness(),awake);
#endif
    if(previous!=duty) { ledcWrite(2,duty); previous=duty; }
}

void panelCommand(uint8_t command) {
    if (displayDma) lcdDmaCommand(command);
    else tft.writecommand(command);
}

void servicePanelPower() {
    const uint32_t now = millis();
    auto* refresh = lv_display_get_refr_timer(display);
    if (!screenAwake && panelPower == PanelPower::Awake) {
        setBacklight(false);
        lv_display_enable_invalidation(display, false);
        lv_timer_pause(refresh);
        if (displayDma) lcdDmaWait();
        panelCommand(0x28); // Display off.
        panelCommand(0x10); // Sleep in.
        panelCommandAt = millis();
        panelPower = PanelPower::Asleep;
        if (activeCpuMhz > IDLE_CPU_MHZ) setCpuFrequencyMhz(IDLE_CPU_MHZ);
    } else if (screenAwake && panelPower == PanelPower::Asleep) {
        if (now - panelCommandAt < PANEL_SLEEP_DELAY_MS) return;
        if (activeCpuMhz > IDLE_CPU_MHZ) setCpuFrequencyMhz(activeCpuMhz);
        panelCommand(0x11); // Sleep out.
        panelCommandAt = millis();
        panelPower = PanelPower::Waking;
    } else if (panelPower == PanelPower::Waking && now - panelCommandAt >= PANEL_SLEEP_DELAY_MS) {
        panelCommand(0x29); // Display on.
        lv_display_enable_invalidation(display, true);
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(display);
        if (displayDma) lcdDmaWait();
        panelPower = PanelPower::Awake;
        setBacklight(true);
    }
}

// Second contact for two-finger map gestures (valid while touchCount == 2).
uint8_t touchCount = 0;
uint16_t touch2X = 0, touch2Y = 0;

bool readTouch(uint16_t& x, uint16_t& y) {
    // FT6336 register 0x02 contains the number of contacts. Point 1 is at 0x03
    // (XH, XL, YH, YL), point 2 at 0x09. Same mapping as the manufacturer's sample.
    Wire.beginTransmission(board::TOUCH_I2C_ADDRESS);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(board::TOUCH_I2C_ADDRESS, static_cast<uint8_t>(11)) != 11) {
        touchCount = 0;
        return false;
    }
    uint8_t r[11];
    for (auto& b : r) b = Wire.read();
    const uint8_t contacts = r[0] & 0x0f;
    if (contacts == 0 || contacts > 2) { touchCount = 0; return false; }
    // Order the two contacts by their touch ID (YH bits 7-4) so a pinch's angle is stable.
    int first = 1, second = 7;
    if (contacts == 2 && (r[3] >> 4) > (r[9] >> 4)) std::swap(first, second);
    auto map = [&](int at) {
        const uint16_t rawX = static_cast<uint16_t>((r[at] & 0x0f) << 8) | r[at + 1];
        const uint16_t rawY = static_cast<uint16_t>((r[at + 2] & 0x0f) << 8) | r[at + 3];
        // TFT_eSPI rotation 1: rotate the native 240 x 320 touch coordinates
        // clockwise into the 320 x 240 HUD coordinate system.
        return huddisplay::touch(rawX, rawY, displayRotation);
    };
    const auto point = map(first);
    x = point.x; y = point.y;
    touchCount = contacts;
    if (contacts == 2) { const auto p2 = map(second); touch2X = p2.x; touch2Y = p2.y; }
    return true;
}

void readTouchInput(lv_indev_t*, lv_indev_data_t* data) {
    static uint16_t x = 0;
    static uint16_t y = 0;
    const bool touching = readTouch(x, y);
    lastTouchDown=touching;
    const bool wasWakeTouch=swallowWakeTouch;
    if (touching) {
        lastTouchAt = millis();
        if (!screenAwake) {
            screenAwake = true;
            // The entire first contact only wakes the screen; it must not
            // activate a music button or tab hidden beneath the finger.
            swallowWakeTouch = true;
            Serial.println("Display awake: touch");
        }
    } else {
        swallowWakeTouch = false;
    }
    bool consumed=false;
    const bool waking = panelPower != PanelPower::Awake;
#ifndef HUD_DEMO_ONLY
    consumed=companionUiTouch(touching,x,y,waking || swallowWakeTouch || wasWakeTouch,touching ? touchCount : 0,touch2X,touch2Y);
#endif
    data->state = touching && !waking && !swallowWakeTouch && !consumed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
}

void serviceDisplayTimeout() {
    // Keep touch and BLE running so they can wake the panel.
    const uint32_t timeout = hudSettingsGet().timeoutSeconds * 1000UL;
#ifndef HUD_DEMO_ONLY
    // A live navigation page must remain readable without touching it while riding.
    if (companionUiNavigationActive() || aiVoiceBusy() || aiVoiceSpeakerBusy()) {
        lastTouchAt = millis();
        screenAwake = true;
    }
    if (companionUiConsumeWakeRequest()) {
        lastTouchAt = millis();
        if (!screenAwake) {
            screenAwake = true;
            Serial.println("Display awake: notification");
        }
    }
#endif
    static uint32_t previousTimeout = timeout;
    if (previousTimeout != timeout) {
        previousTimeout = timeout;
        lastTouchAt = millis(); screenAwake = true;
    }
    if (touchInput && screenAwake && timeout && uint32_t(millis() - lastTouchAt) >= timeout) {
        screenAwake = false;
        Serial.printf("Display asleep: %us without touch\n", unsigned(timeout / 1000));
    }
}

// Blocking flush. TFT_eSPI's DMA path (pushImageDMA) only delivered the first strip
// on this ESP32-S3 board and left the rest of the screen black, so it is not used.
void flushDisplay(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, width, height);
    tft.pushColors(reinterpret_cast<uint16_t*>(pixels), width * height, true);
    tft.endWrite();
    lv_display_flush_ready(display);
}

void initDisplay() {
    pinMode(board::LCD_BACKLIGHT, OUTPUT);
    digitalWrite(board::LCD_BACKLIGHT,HIGH);

    tft.begin();
    initIli9341vPanel(tft);
    displayRotation=hudHardwareSettingsGet().rotation;
    tft.setRotation((TFT_ROTATION+displayRotation)%4);
    tft.fillScreen(TFT_BLACK);
    ledcSetup(2,5000,8); ledcAttachPin(board::LCD_BACKLIGHT,2); setBacklight(true);

    lv_init();
    display = lv_display_create(tft.width(), tft.height());
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // Hand the bus to esp_lcd for DMA transfers; TFT_eSPI only did the panel init above.
    displayDma = lcdDmaBegin(display, sizeof(displayBuffer));
    Serial.printf("Display transfer: %s\n", displayDma ? "esp_lcd DMA" : "TFT_eSPI blocking");
    if (displayDma) {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
        lv_display_set_buffers(display, displayBuffer, displayBuffer2, sizeof(displayBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, lcdDmaFlush);
    } else {
        lv_display_set_buffers(display, displayBuffer, nullptr, sizeof(displayBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, flushDisplay);
    }
}
void serviceHardwareSettings() {
    const auto& hardware=hudHardwareSettingsGet();
    static int previousVolume=-1;
    if(previousVolume!=hardware.speakerVolume && aiVoiceSetSpeakerVolume(hardware.speakerVolume)) previousVolume=hardware.speakerVolume;
    if(displayRotation!=hardware.rotation && panelPower==PanelPower::Awake && !lastTouchDown && !aiVoiceBusy()) {
        displayRotation=hardware.rotation;
        if(displayDma) {
            // TFT_eSPI no longer owns the bus: rotate with MADCTL and repaint everything.
            lcdDmaWait();
            lcdDmaSetMadctl(MADCTL[(TFT_ROTATION+displayRotation)%4]);
            lv_obj_invalidate(lv_screen_active());
        } else {
            tft.setRotation((TFT_ROTATION+displayRotation)%4); tft.fillScreen(TFT_BLACK);
            lv_display_set_resolution(display,tft.width(),tft.height());
            lv_display_set_buffers(display,displayBuffer,nullptr,sizeof(displayBuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
        }
        if(touchInput) lv_indev_reset(touchInput,nullptr);
        companionUiLayout(tft.width(),tft.height());
        lastTouchAt=millis(); swallowWakeTouch=true;
    }
}

void initTouch() {
    pinMode(board::TOUCH_RST, OUTPUT);
    digitalWrite(board::TOUCH_RST, HIGH);
    delay(10);
    digitalWrite(board::TOUCH_RST, LOW);
    delay(20);
    digitalWrite(board::TOUCH_RST, HIGH);
    delay(200);

    pinMode(board::TOUCH_INT, INPUT_PULLUP);
    Wire.begin(board::TOUCH_SDA, board::TOUCH_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(20);
    Wire.beginTransmission(board::TOUCH_I2C_ADDRESS);
    if (Wire.endTransmission() != 0) {
        Serial.println("FT6336 not detected; running without touch");
        return;
    }

    touchInput = lv_indev_create();
    lv_indev_set_type(touchInput, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touchInput, readTouchInput);
}

void pulseRandomButton() {
    static lv_obj_t* const buttons[] = {
        objects.btn_left,
        objects.btn_headlight,
        objects.btn_checkengine,
        objects.btn_oil,
        objects.btn_temp,
        objects.btn_right
    };

    if (pressedButton != nullptr) {
        lv_obj_remove_state(pressedButton, LV_STATE_PRESSED);
    }

    pressedButton = buttons[random(0, sizeof(buttons) / sizeof(buttons[0]))];
    lv_obj_add_state(pressedButton, LV_STATE_PRESSED);
    buttonPressedUntil = millis() + BUTTON_PULSE_MS;
}

void setFloatLabel(lv_obj_t* label, float value, uint8_t decimals,
                   const char* suffix = "") {
    char text[24];
    char format[8];
    snprintf(format, sizeof(format), "%%.%uf%%s", decimals);
    snprintf(text, sizeof(text), format, value, suffix);
    lv_label_set_text(label, text);
}

void updateGearStyle(int gear) {
    lv_obj_set_style_text_color(
        objects.label_numbergear,
        gear == 0 ? lv_color_hex(0x00ff1d) : lv_color_white(),
        LV_PART_MAIN | LV_STATE_DEFAULT);
}

void updateGasLevel(int gasLevel) {
    lv_bar_set_value(objects.bar_gaslevel, gasLevel, LV_ANIM_OFF);

    const lv_color_t color = gasLevel < 30
        ? lv_color_hex(0xff2020)
        : gasLevel < 70
            ? lv_color_hex(0xf3a621)
            : lv_color_hex(0x2080ff);
    lv_obj_set_style_bg_color(
        objects.bar_gaslevel, color, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

void updateRandomHud() {
    char text[24];

    const int gear = random(0, 6);
    snprintf(text, sizeof(text), "%c", gear == 0 ? 'N' : static_cast<char>('0' + gear));
    lv_label_set_text(objects.label_numbergear, text);
    updateGearStyle(gear);

    lv_label_set_text_fmt(objects.label_speed, "%d", random(0, 181));
    lv_label_set_text_fmt(objects.label_time, "%02d:%02d",
                          random(0, 24), random(0, 60));
    setFloatLabel(objects.label_dcvolt, random(118, 145) / 10.0f, 1, "V");
    setFloatLabel(objects.label_trip, random(0, 5000) / 10.0f, 1);
    lv_label_set_text_fmt(objects.label_odo, "%d",
                          random(1000, 120000));
    setFloatLabel(objects.label_avg, random(20, 1000) / 10.0f, 1);
    lv_label_set_text_fmt(objects.label_temp, "%d",
                          random(20, 121));
    updateGasLevel(random(0, 101));
    lv_bar_set_value(objects.bar_rpm, random(0, 101), LV_ANIM_OFF);

    pulseRandomButton();
}

void updateRandomHudState() {
    const uint32_t now = millis();
    if (pressedButton != nullptr &&
        static_cast<int32_t>(now - buttonPressedUntil) >= 0) {
        lv_obj_remove_state(pressedButton, LV_STATE_PRESSED);
        pressedButton = nullptr;
    }

    if (now - lastHudUpdate >= HUD_UPDATE_INTERVAL_MS) {
        lastHudUpdate = now;
        updateRandomHud();
    }
}


// Settings are stored in NVS; a full partition makes every save roll back.
void printNvs() {
    nvs_stats_t stats{};
    if (nvs_get_stats(nullptr, &stats) == ESP_OK)
        Serial.printf("NVS: used=%u free=%u total=%u namespaces=%u\n", unsigned(stats.used_entries),
                      unsigned(stats.free_entries), unsigned(stats.total_entries), unsigned(stats.namespace_count));
    nvs_iterator_t it = nvs_entry_find("nvs", nullptr, NVS_TYPE_ANY);
    while (it) {
        nvs_entry_info_t info; nvs_entry_info(it, &info);
        size_t used = 0; nvs_handle_t h;
        if (nvs_open(info.namespace_name, NVS_READONLY, &h) == ESP_OK) {
            if (info.type == NVS_TYPE_BLOB) nvs_get_blob(h, info.key, nullptr, &used);
            else if (info.type == NVS_TYPE_STR) nvs_get_str(h, info.key, nullptr, &used);
            nvs_close(h);
        }
        Serial.printf("NVS entry: %s/%s type=%u bytes=%u\n", info.namespace_name, info.key, unsigned(info.type), unsigned(used));
        it = nvs_entry_next(it);
    }
    nvs_release_iterator(it);
}

// Removes namespaces left by an older firmware. Keeps this firmware's settings,
// BLE bonds (iPhone pairing) and RF calibration.
void cleanNvs() {
    const char* keep[] = {"hud-settings", "nimble_bond", "phy"};
    char names[32][16]{}; int count = 0;
    nvs_iterator_t it = nvs_entry_find("nvs", nullptr, NVS_TYPE_ANY);
    while (it) {
        nvs_entry_info_t info; nvs_entry_info(it, &info);
        bool known = false;
        for (const char* k : keep) known = known || !strcmp(info.namespace_name, k);
        for (int i = 0; i < count; ++i) known = known || !strcmp(names[i], info.namespace_name);
        if (!known && count < 32) strlcpy(names[count++], info.namespace_name, sizeof(names[0]));
        it = nvs_entry_next(it);
    }
    nvs_release_iterator(it);
    for (int i = 0; i < count; ++i) {
        nvs_handle_t h;
        esp_err_t err = nvs_open(names[i], NVS_READWRITE, &h);
        if (err == ESP_OK) { err = nvs_erase_all(h); if (err == ESP_OK) err = nvs_commit(h); nvs_close(h); }
        Serial.printf("NVSCLEAN: %s %s\n", names[i], err == ESP_OK ? "erased" : esp_err_to_name(err));
    }
    printNvs();
}

bool displayBenchPending = false;

void benchmarkDisplay() {
    // Full-screen redraw cost (render + transfer), averaged over 20 frames.
    uint32_t total = 0, worst = 0;
    for (int i = 0; i < 20; ++i) {
        lv_obj_invalidate(lv_screen_active());
        const uint32_t start = micros();
        lv_refr_now(display);
        if (displayDma) lcdDmaWait();
        const uint32_t us = micros() - start;
        total += us;
        worst = std::max(worst, us);
    }
    Serial.printf("BENCH full frame: avg=%ums worst=%ums (%.1f fps)\n", unsigned(total / 20000), unsigned(worst / 1000),
                  20e6 / double(total));
}

void handleSerialCommands() {
    static char command[96];
    static size_t size = 0;
    while (Serial.available()) {
        const char value = Serial.read();
        if (value == '\r') continue;
        if (value != '\n') {
            if (size < sizeof(command) - 1) command[size++] = value;
            continue;
        }
        command[size] = '\0';
        if (strcmp(command, "NVSCLEAN") == 0) {
            cleanNvs();
        } else if (strcmp(command, "NVS") == 0) {
            printNvs();
        } else if (strcmp(command, "STATUS") == 0) {
            Serial.printf("HUD iPhone running; heap=%u; touch=%s\n", ESP.getFreeHeap(),
                          touchInput ? "FT6336 ready" : "unavailable");
            phoneLinkPrintStatus();
            companionUiPrintStatus();
            hudSettingsPrintStatus();
            aiVoicePrintStatus();
            Serial.printf("Display idle: awake=%u; backlight=%u; idle_ms=%u; timeout_ms=%u; panel=%s; cpu_mhz=%u\n",
                          screenAwake, ledcRead(2),
                          uint32_t(millis() - lastTouchAt), hudSettingsGet().timeoutSeconds * 1000UL,
                          panelPower==PanelPower::Asleep ? "asleep" : panelPower==PanelPower::Waking ? "waking" : "awake",
                          unsigned(getCpuFrequencyMhz()));
        } else if (strcmp(command, "JPEGBENCH") == 0) {
            companionUiJpegBench();
        } else if (strcmp(command, "BENCH") == 0) {
            lastTouchAt = millis();
            screenAwake = true;
            displayBenchPending = true;
        } else if (strcmp(command, "REFRESH") == 0) {
            const auto result = phoneLinkRefreshNotifications();
            Serial.printf("ANCS refresh request: %u (0=queued, 1=offline, 2=pairing, 3=busy)\n",
                          unsigned(result));
        } else if (strncmp(command, "TIME ", 5) == 0) {
            long long epoch;
            int offsetMinutes;
            if (sscanf(command + 5, "%lld %d", &epoch, &offsetMinutes) == 2 &&
                epoch >= 1577836800LL && epoch <= 4102444800LL &&
                offsetMinutes >= -720 && offsetMinutes <= 960) {
                phone::ClockSample sample;
                sample.utcSeconds = epoch;
                sample.utcOffsetSeconds = offsetMinutes * 60;
                companionUiSetTime(sample, false);
                Serial.println("Clock set over USB; waiting for iPhone CTS sync");
            } else {
                Serial.println("Usage: TIME <unix-seconds> <UTC-offset-minutes>");
            }
        }
        size = 0;
    }
}
}

void setup() {
    Serial.begin(115200);
    delay(200);
    activeCpuMhz = getCpuFrequencyMhz();
    randomSeed(micros());
    wallpaperBegin(); // before settings: migrating the old wallpaper face writes its prefs
    hudSettingsBegin();
    initDisplay();
    initTouch();
    aiVoiceBegin();
#ifdef HUD_DEMO_ONLY
    ui_init();
    updateRandomHud();
#else
    batteryBegin();
    companionUiBegin();
    companionUiLayout(tft.width(),tft.height());
    lv_mem_monitor_t memory{}; lv_mem_monitor(&memory);
    Serial.printf("LVGL PSRAM arena: total=%u; free=%u; largest=%u\n",
        unsigned(memory.total_size),unsigned(memory.free_size),unsigned(memory.free_biggest_size));
    if (!phoneLinkBegin()) {
        Serial.println("Failed to initialize iPhone BLE link");
        companionUiBluetoothFailed();
    }
#endif
    Serial.printf("Display: %u x %u; flash: %u MB; PSRAM: %u KiB\n",
                  SCREEN_WIDTH, SCREEN_HEIGHT,
                  ESP.getFlashChipSize() / (1024 * 1024),
                  ESP.getPsramSize() / 1024);
    Serial.println("HUD iPhone ready");
    lastTouchAt = millis();
}

void loop() {
    static uint32_t lastTick = millis();
    const uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;

#ifdef HUD_DEMO_ONLY
    ui_tick();
    updateRandomHudState();
#else
    hudSettingsTick();
    serviceHardwareSettings();
    PhoneEvent event;
    while (phoneLinkPoll(event)) companionUiHandle(event);
    handleSerialCommands();
    batteryTick(now, ledcRead(2));
    companionUiTick();
#endif
    const uint32_t frameStart = millis();
    lv_timer_handler();
    // Measurement aid: a map JPEG decode or full-screen flush shows up here.
    const uint32_t frameMs = millis() - frameStart;
    if (frameMs >= 60) Serial.printf("LVGL slow frame: %ums\n", unsigned(frameMs));
    serviceDisplayTimeout();
    servicePanelPower();
    if (panelPower == PanelPower::Awake) {
        if (displayBenchPending) {
            displayBenchPending = false;
            benchmarkDisplay();
        }
        static uint32_t backlightAt=0, backlightRevision=UINT32_MAX;
        const uint32_t revision = hudSettingsRevision();
        if (now-backlightAt>=1000 || revision!=backlightRevision) {
            backlightAt=now;
            backlightRevision=revision;
            setBacklight(true);
        }
    }
    delay(panelPower==PanelPower::Asleep && !screenAwake ? 20 : 5);
}
