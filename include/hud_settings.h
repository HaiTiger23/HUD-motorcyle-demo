#pragma once
#include <stdint.h>
class NimBLEService;
struct HudSettings {
    uint8_t theme = 0;
    uint8_t face = 0;
    uint16_t timeoutSeconds = 30;
    bool allowNewApps = true;
};
struct HudHardwareSettings {
    uint8_t brightness,speakerVolume,rotation;
    HudHardwareSettings(uint8_t b=100,uint8_t v=30,uint8_t r=0):brightness(b),speakerVolume(v),rotation(r) {}
};
const HudHardwareSettings& hudHardwareSettingsGet();
void hudSettingsBegin();
void hudSettingsAttach(NimBLEService* service);
void hudSettingsTick();
const HudSettings& hudSettingsGet();
bool hudSettingsApply(const HudSettings& settings);
uint32_t hudSettingsRevision();
bool hudSettingsAllowNotification(const char* app);
void hudSettingsPrintStatus();
const char* hudSettingsAppName(const char* identifier);
void hudSettingsSetAppName(const char* identifier,const char* name);
void hudSettingsRequestAppNames();
