#include "nav_protocol.h"
#include <string.h>

namespace hudnav {
namespace {
uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
}
bool decodeMapMotion(const uint8_t* data, size_t size, MapMotion& out) {
    if (!data || (size != 8 && size != 15) || data[0] != 7 || data[7] > 3) return false;
    const uint16_t speed = le16(data + 5);
    if (speed < 10 || speed > 2000) return false;
    const uint8_t margin = size == 8 ? LEGACY_MOTION_MARGIN : data[8];
    if (margin < LEGACY_MOTION_MARGIN || margin > MAX_MOTION_MARGIN) return false;
    out = {};
    out.frame = le32(data + 1);
    out.speed = speed;
    out.marginX = size == 8 ? LEGACY_MOTION_MARGIN : 0;
    out.marginY = margin;
    out.arrow = (data[7] & 1) != 0;
    out.light = (data[7] & 2) != 0;
    if (size == 15) {
        out.previousFrame = le32(data + 9);
        const int advance = le16(data + 13);
        out.advance = advance < 0x8000 ? advance : advance - 0x10000;
    }
    return true;
}
int MapMotion::offset(uint32_t ageMs, int correction, uint32_t correctionAgeMs) const {
    const auto predicted = [this](uint32_t time) {
        const uint32_t age = time > 1200 ? 1200 : time;
        const int pixels = (speed * age + 5000) / 10000;
        return pixels > marginY ? int(marginY) : pixels;
    };
    const int shift = predicted(ageMs);
    if (correction > 0) {
        // Let the camera catch up to a frame that starts ahead of its GPS target.
        // Consume at most half the forward motion so a straight road cannot reverse.
        const int moved = shift - predicted(ageMs > correctionAgeMs ? ageMs - correctionAgeMs : 0);
        const int remaining = correction > moved / 2 ? correction - moved / 2 : 0;
        return shift + remaining > marginY ? marginY : shift + remaining;
    }
    constexpr int fadeMs = 350;
    const int remaining = correctionAgeMs >= fadeMs ? 0 : fadeMs - int(correctionAgeMs);
    const int adjusted = shift + correction * remaining / fadeMs;
    if (adjusted < -marginY) return -marginY;
    return adjusted;
}
bool decodeTrip(const uint8_t* data, size_t size, TripSummary& out) {
    constexpr size_t header = 12;
    if (!data || size < header || data[0] != 1) return false;
    out = TripSummary{};
    out.remainingMetres = le32(data + 1);
    out.turnMetres = le32(data + 5);
    out.etaMinute = le16(data + 9);
    out.maneuver = data[11];
    const auto* text = data + header;
    const auto* separator = static_cast<const uint8_t*>(memchr(text, 0, size - header));
    const size_t length = separator ? size_t(separator - text) : size - header;
    size_t clipped = length < sizeof(out.instruction) ? length : sizeof(out.instruction) - 1;
    while (clipped && clipped < length && (text[clipped] & 0xC0) == 0x80) --clipped;
    memcpy(out.instruction, text, clipped);
    if (separator && size - header - length == 5 && separator[1] <= 1) {
        out.simulating = separator[1] == 1;
        const uint8_t maneuver = separator[2];
        const uint16_t gap = le16(separator + 3);
        if (((maneuver >= 1 && maneuver <= 5) || (maneuver >= 7 && maneuver <= 16)) && gap >= 4 && gap <= 100) {
            out.followingManeuver = maneuver;
            out.followingMetres = gap;
        }
    }
    return true;
}
uint32_t crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
bool validDimensions(const uint8_t* p, size_t size, uint16_t width, uint16_t height) {
    if (size < 4 || p[0] != 0xff || p[1] != 0xd8) return false;
    size_t i = 2;
    while (i + 4 <= size) {
        if (p[i++] != 0xff) return false;
        while (i < size && p[i] == 0xff) ++i;
        if (i >= size) return false;
        const uint8_t marker = p[i++];
        if (marker == 0xda || marker == 0xd9) return false;
        if (i + 2 > size) return false;
        const size_t length = size_t(p[i]) * 256 + p[i + 1];
        if (length < 2 || length > size - i) return false;
        if (marker == 0xc0) {
            return length >= 8 && p[i + 2] == 8 &&
                (uint16_t(p[i + 3]) * 256 + p[i + 4]) == height &&
                (uint16_t(p[i + 5]) * 256 + p[i + 6]) == width &&
                (p[i + 7] == 1 || p[i + 7] == 3);
        }
        if (marker >= 0xc1 && marker <= 0xc3) return false; // baseline JPEG only
        i += length;
    }
    return false;
}
Result Receiver::feed(const uint8_t* p, size_t size) {
    if (!p || !size) return Result::Rejected;
    if (p[0] == 1) {
        reset();
        if (size != 13) return Result::Rejected;
        id_ = le32(p + 1); total_ = le32(p + 5); checksum_ = le32(p + 9);
        if (!total_ || total_ > capacity_ || total_ > MAX_IMAGE) { reset(); return Result::Rejected; }
        active_ = true;
        return Result::Partial;
    }
    if (!active_ || size < 5 || le32(p + 1) != id_) return Result::Rejected;
    if (p[0] == 2 && size > 9) {
        if (le32(p + 5) != received_ || size - 9 > total_ - received_) {
            reset(); return Result::Rejected;
        }
        memcpy(buffer_ + received_, p + 9, size - 9);
        received_ += size - 9;
        return Result::Partial;
    }
    if (p[0] == 3 && size == 5) {
        active_ = false;
        if (received_ == total_ && crc32(buffer_, total_) == checksum_ && validDimensions(buffer_, total_, width_, height_))
            return Result::Complete;
    }
    reset();
    return Result::Rejected;
}
}
