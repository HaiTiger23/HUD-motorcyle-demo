#include "nav_protocol.h"
#include <string.h>

namespace hudnav {
namespace {
uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
}
uint32_t crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
bool validDimensions(const uint8_t* p, size_t size) {
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
                (uint16_t(p[i + 3]) * 256 + p[i + 4]) == HEIGHT &&
                (uint16_t(p[i + 5]) * 256 + p[i + 6]) == WIDTH &&
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
        if (received_ == total_ && crc32(buffer_, total_) == checksum_ && validDimensions(buffer_, total_))
            return Result::Complete;
    }
    reset();
    return Result::Rejected;
}
}
