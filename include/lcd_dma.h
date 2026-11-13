#pragma once
#include <lvgl.h>

// ILI9341 pixel transfer through ESP-IDF esp_lcd (SPI2 + DMA). TFT_eSPI still runs the
// panel's power-on init; afterwards this module owns the SPI pins and TFT_eSPI must
// not touch the display again.
bool lcdDmaBegin(lv_display_t* display, size_t maxTransferBytes);
void lcdDmaFlush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
void lcdDmaSetMadctl(uint8_t value);   // rotation / colour order register
void lcdDmaCommand(uint8_t command);
void lcdDmaWait();                     // until queued pixel transfers are done
