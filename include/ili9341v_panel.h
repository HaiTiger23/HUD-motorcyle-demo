#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Register values from the vendor's Arduino/Replaced files/ILI9341_Init.h.
// This IPS panel needs different power, gamma and inversion settings from
// TFT_eSPI's generic ILI9341 profile. Keep the override in the project itself.
inline void initIli9341vPanel(TFT_eSPI& panel) {
    struct Command {
        uint8_t address;
        uint8_t length;
        uint8_t data[15];
    };
    static const Command commands[] = {
        {0xCF, 3, {0x00, 0xC1, 0x30}},
        {0xED, 4, {0x64, 0x03, 0x12, 0x81}},
        {0xE8, 3, {0x85, 0x00, 0x78}},
        {0xCB, 5, {0x39, 0x2C, 0x00, 0x34, 0x02}},
        {0xF7, 1, {0x20}},
        {0xEA, 2, {0x00, 0x00}},
        {0xC0, 1, {0x13}},
        {0xC1, 1, {0x13}},
        {0xC5, 2, {0x22, 0x35}},
        {0xC7, 1, {0xBD}},
        {0x21, 0, {}},
        {0x36, 1, {0x08}},
        {0xB6, 2, {0x08, 0x82}},
        {0x3A, 1, {0x55}},
        {0xF6, 2, {0x01, 0x30}},
        {0xB1, 2, {0x00, 0x1B}},
        {0xF2, 1, {0x00}},
        {0x26, 1, {0x01}},
        {0xE0, 15, {0x0F, 0x35, 0x31, 0x0B, 0x0E, 0x06, 0x49, 0xA7,
                    0x33, 0x07, 0x0F, 0x03, 0x0C, 0x0A, 0x00}},
        {0xE1, 15, {0x00, 0x0A, 0x0F, 0x04, 0x11, 0x08, 0x36, 0x58,
                    0x4D, 0x07, 0x10, 0x0C, 0x32, 0x34, 0x0F}},
    };

    panel.writecommand(0x01);  // Software reset; hardware reset is shared.
    delay(150);
    panel.startWrite();
    for (const auto& command : commands) {
        panel.writecommand(command.address);
        for (uint8_t i = 0; i < command.length; ++i) {
            panel.writedata(command.data[i]);
        }
    }
    panel.writecommand(0x11);  // Sleep out.
    panel.endWrite();
    delay(120);
    panel.writecommand(0x29);  // Display on.
}
