#pragma once
#include <stddef.h>
#include <stdint.h>

// Photo background, usable behind every clock face: a 320x240 JPEG kept in the
// LittleFS partition and decoded once into RGB565 (PSRAM).
constexpr int WALL_W = 320, WALL_H = 240;
void wallpaperBegin();
bool wallpaperHas();
const uint16_t* wallpaperPixels();       // valid when wallpaperHas()
uint32_t wallpaperRevision();            // bumps whenever the image or colour changes
bool wallpaperSave(const uint8_t* jpeg, size_t size);
void wallpaperClear();
uint32_t wallpaperColor();               // 0xRRGGBB for the clock
void wallpaperSetColor(uint32_t rgb);
// Background style for all faces: false = theme colour, true = the photo.
bool wallpaperBackground();
void wallpaperSetBackground(bool photo);
// LittleFS usage for the device-info screen; false when the partition is not mounted.
bool wallpaperStorage(size_t& used, size_t& total);
// Copies the stored wallpaper JPEG (for benchmarks); returns its size or 0.
size_t wallpaperReadJpeg(uint8_t* out, size_t capacity);
