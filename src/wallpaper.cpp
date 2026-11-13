#include "wallpaper.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <src/libs/tjpgd/tjpgd.h>

namespace {
constexpr const char* PATH = "/wall.jpg";
uint16_t* pixels = nullptr;
bool has = false, mounted = false, background = false;
uint32_t revision = 1, color = 0xFFFFFF;
Preferences prefs;

struct Source { const uint8_t* data; size_t left; uint16_t* out; };
size_t input(JDEC* jd, uint8_t* buffer, size_t size) {
    auto* s = static_cast<Source*>(jd->device);
    if (size > s->left) size = s->left;
    if (buffer) memcpy(buffer, s->data, size);
    s->data += size; s->left -= size;
    return size;
}
int output(JDEC* jd, void* bitmap, JRECT* rect) {
    auto* s = static_cast<Source*>(jd->device);
    if (rect->right >= WALL_W || rect->bottom >= WALL_H) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(bitmap);
    for (unsigned y = rect->top; y <= rect->bottom; ++y) {
        uint16_t* row = s->out + y * WALL_W;
        for (unsigned x = rect->left; x <= rect->right; ++x, p += 3)
            row[x] = uint16_t(((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[0] >> 3));
    }
    return 1;
}
bool decode(const uint8_t* jpeg, size_t size, uint16_t* out) {
    static uint32_t pool[1024];
    Source source{jpeg, size, out};
    JDEC decoder;
    if (jd_prepare(&decoder, input, pool, sizeof(pool), &source) != JDR_OK) return false;
    if (decoder.width != WALL_W || decoder.height != WALL_H) return false;
    return jd_decomp(&decoder, output, 0) == JDR_OK;
}
}

void wallpaperBegin() {
    prefs.begin("hud-wall", false);
    color = prefs.getUInt("color", 0xFFFFFF) & 0xFFFFFF;
    background = prefs.getBool("photo", false);
    pixels = static_cast<uint16_t*>(heap_caps_malloc(WALL_W * WALL_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    // The 16 MB layout's unused "spiffs" partition; formatted on first use.
    mounted = LittleFS.begin(true);
    if (!mounted || !pixels || !LittleFS.exists(PATH)) {
        Serial.printf("Wallpaper: fs=%u image=0\n", unsigned(mounted));
        return;
    }
    File file = LittleFS.open(PATH, "r");
    const size_t size = file ? file.size() : 0;
    uint8_t* jpeg = size && size <= 64 * 1024 ? static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM)) : nullptr;
    if (jpeg && file.read(jpeg, size) == size) has = decode(jpeg, size, pixels);
    if (jpeg) heap_caps_free(jpeg);
    file.close();
    Serial.printf("Wallpaper: fs=1 image=%u bytes=%u\n", unsigned(has), unsigned(size));
}

bool wallpaperHas() { return has; }
const uint16_t* wallpaperPixels() { return pixels; }
uint32_t wallpaperRevision() { return revision; }
uint32_t wallpaperColor() { return color; }

bool wallpaperSave(const uint8_t* jpeg, size_t size) {
    if (!pixels || !decode(jpeg, size, pixels)) { Serial.println("Wallpaper: decode failed"); return false; }
    has = true; ++revision;
    bool saved = false;
    if (mounted) {
        File file = LittleFS.open(PATH, "w");
        saved = file && file.write(jpeg, size) == size;
        file.close();
    }
    Serial.printf("Wallpaper: received %u bytes; saved=%u\n", unsigned(size), unsigned(saved));
    return true;
}

void wallpaperClear() {
    if (mounted) LittleFS.remove(PATH);
    has = false; ++revision;
}

void wallpaperSetColor(uint32_t rgb) {
    rgb &= 0xFFFFFF;
    if (rgb == color) return;
    color = rgb; prefs.putUInt("color", rgb); ++revision;
}

bool wallpaperBackground() { return background; }
void wallpaperSetBackground(bool photo) {
    if (photo == background) return;
    background = photo; prefs.putBool("photo", photo); ++revision;
}
bool wallpaperStorage(size_t& used, size_t& total) {
    if (!mounted) return false;
    used = LittleFS.usedBytes(); total = LittleFS.totalBytes();
    return true;
}
size_t wallpaperReadJpeg(uint8_t* out, size_t capacity) {
    if (!mounted || !LittleFS.exists(PATH)) return 0;
    File file = LittleFS.open(PATH, "r");
    const size_t size = file ? file.size() : 0;
    const size_t got = size && size <= capacity ? file.read(out, size) : 0;
    file.close();
    return got == size ? size : 0;
}
