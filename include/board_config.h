#pragma once

#include <stdint.h>

// 2.8 inch ESP32-S3 Display (ES3C28P / ES3N28P), as documented by the vendor.
// The panel is a 240 x 320 ILI9341V used in landscape orientation (320 x 240).
namespace board {
constexpr int LCD_MISO = 13;
constexpr int LCD_MOSI = 11;
constexpr int LCD_SCLK = 12;
constexpr int LCD_CS = 10;
constexpr int LCD_DC = 46;
constexpr int LCD_RST = -1;  // LCD reset is wired to CHIP_PU on this board.
constexpr int LCD_BACKLIGHT = 45;

constexpr int TOUCH_SCL = 15;
constexpr int TOUCH_SDA = 16;
constexpr int TOUCH_INT = 17;
constexpr int TOUCH_RST = 18;
constexpr uint8_t TOUCH_I2C_ADDRESS = 0x38;
constexpr int BAT_ADC = 9;
}
