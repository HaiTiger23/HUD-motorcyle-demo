#pragma once

#include <stddef.h>
#include <stdint.h>

namespace phone {
struct Notice {
    uint32_t uid = 0;
    uint8_t category = 0;
    bool preExisting = false;
    char app[96] = {};
    char title[160] = {};
    char message[512] = {};
    char date[16] = {};
    char negativeAction[64] = {};
    bool canDismiss = false;
    bool negativeActionAvailable = false;
    uint32_t actionSession = 0;
};

// ANCS responses may be fragmented at any byte, including inside UTF-8 text.
// The parser consumes declared attribute lengths but bounds stored text.
class AncsParser {
public:
    void begin(uint32_t uid, uint8_t category, bool metadata = false);
    bool feed(const uint8_t* bytes, size_t size);
    bool complete() const { return complete_; }
    bool failed() const { return failed_; }
    const Notice& notice() const { return notice_; }

private:
    Notice notice_;
    uint8_t header_[5] = {};
    uint8_t headerSize_ = 0;
    uint8_t tuple_[3] = {};
    uint8_t tupleSize_ = 0;
    uint8_t attribute_ = 0;
    uint8_t received_ = 0;
    uint16_t length_ = 0;
    uint16_t consumed_ = 0;
    bool complete_ = false;
    bool failed_ = false;
    bool metadata_ = false;
    void finishAttribute();
};

class AppNameParser {
public:
    void begin(const char* identifier);
    bool feed(const uint8_t* bytes,size_t length);
    bool failed() const { return failed_; }
    const char* identifier() const { return expected_; }
    const char* name() const { return name_; }
private:
    char expected_[96] = {};
    char echoed_[96] = {};
    char name_[96] = {};
    uint16_t position_ = 0, consumed_ = 0, length_ = 0;
    uint8_t stage_ = 0;
    bool failed_ = false, complete_ = false;
};

struct ClockSample {
    int64_t utcSeconds = 0;
    int32_t utcOffsetSeconds = 7 * 3600;
};

bool decodeCurrentTime(const uint8_t* data, size_t size, int32_t offsetSeconds,
                       ClockSample& sample);
bool decodeLocalTime(const uint8_t* data, size_t size, int32_t& offsetSeconds);
bool isGoogleMaps(const char* appIdentifier);
bool isDismissAction(uint8_t category, const char* label);
uint64_t notificationDateKey(const char* date);

enum class MusicField : uint8_t { Player, Playback, Volume, Artist, Title };
struct MusicUpdate {
    MusicField field = MusicField::Player;
    bool truncated = false;
    char text[160] = {};
};
bool decodeMusicUpdate(const uint8_t* data, size_t size, MusicUpdate& update);
uint16_t decodeMusicCommands(const uint8_t* data, size_t size);
}
