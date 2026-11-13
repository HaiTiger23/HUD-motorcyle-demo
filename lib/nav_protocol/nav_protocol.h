#pragma once
#include <stddef.h>
#include <stdint.h>

namespace hudnav {
constexpr size_t MAX_IMAGE = 48 * 1024;
constexpr uint16_t WIDTH = 320, HEIGHT = 240; // full-screen map frames
constexpr uint8_t LEGACY_MOTION_MARGIN = 12, MAX_MOTION_MARGIN = 64;
struct MapMotion {
    uint32_t frame = 0, startedAt = 0;
    uint32_t previousFrame = 0;
    uint16_t speed = 0;  // tenths of a screen pixel per second
    int16_t advance = 0; // previous camera centre in this image, relative to its centre
    uint8_t marginX = 0, marginY = 0;
    bool arrow = false, light = false;
    uint16_t width() const { return WIDTH + 2 * marginX; }
    uint16_t height() const { return HEIGHT + 2 * marginY; }
    int offset(uint32_t ageMs, int correction = 0, uint32_t correctionAgeMs = 0) const;
};
bool decodeMapMotion(const uint8_t* data, size_t size, MapMotion& out);
uint32_t crc32(const uint8_t* data, size_t size);
bool validDimensions(const uint8_t* data, size_t size, uint16_t width = WIDTH, uint16_t height = HEIGHT);
struct TripSummary {
    uint32_t remainingMetres = 0, turnMetres = 0;
    uint16_t etaMinute = 0xFFFF;
    uint8_t maneuver = 0;
    char instruction[152] = {};
    bool simulating = false;
    uint8_t followingManeuver = 255;
    uint16_t followingMetres = 0;
};
bool decodeTrip(const uint8_t* data, size_t size, TripSummary& out);
// Full-screen wallpaper frames reuse the map transfer.
constexpr uint16_t WALL_WIDTH = 320, WALL_HEIGHT = 240;
enum class Result { Partial, Complete, Rejected };
// Owns no memory. Caller supplies the incoming image buffer and serializes access.
class Receiver {
public:
    Receiver(uint8_t* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {}
    Result feed(const uint8_t* data, size_t size);
    void reset() { active_ = false; received_ = total_ = 0; }
    size_t size() const { return total_; }
    uint32_t id() const { return id_; }
    // Dimensions the next completed frame must have (map frames by default).
    void expect(uint16_t width, uint16_t height) { width_ = width; height_ = height; }
private:
    uint8_t* buffer_;
    size_t capacity_, received_ = 0, total_ = 0;
    uint32_t id_ = 0, checksum_ = 0;
    bool active_ = false;
    uint16_t width_ = WIDTH, height_ = HEIGHT;
};
}
