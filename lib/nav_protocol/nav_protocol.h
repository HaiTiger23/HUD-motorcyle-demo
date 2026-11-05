#pragma once
#include <stddef.h>
#include <stdint.h>

namespace hudnav {
constexpr size_t MAX_IMAGE = 48 * 1024;
constexpr uint16_t WIDTH = 300, HEIGHT = 200;
uint32_t crc32(const uint8_t* data, size_t size);
bool validDimensions(const uint8_t* data, size_t size);
enum class Result { Partial, Complete, Rejected };
// Owns no memory. Caller supplies the incoming image buffer and serializes access.
class Receiver {
public:
    Receiver(uint8_t* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {}
    Result feed(const uint8_t* data, size_t size);
    void reset() { active_ = false; received_ = total_ = 0; }
    size_t size() const { return total_; }
    uint32_t id() const { return id_; }
private:
    uint8_t* buffer_;
    size_t capacity_, received_ = 0, total_ = 0;
    uint32_t id_ = 0, checksum_ = 0;
    bool active_ = false;
};
}
