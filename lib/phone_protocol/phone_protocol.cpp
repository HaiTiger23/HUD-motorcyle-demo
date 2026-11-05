#include "phone_protocol.h"

#include <string.h>

namespace phone {
namespace {
// Trim a trailing partial UTF-8 codepoint after a bounded copy.
void trimUtf8(char* text, size_t capacity) {
    const size_t size = strlen(text);
    if (!size) return;
    size_t lead = size - 1;
    while (lead > 0 && (static_cast<uint8_t>(text[lead]) & 0xC0) == 0x80) --lead;
    const uint8_t byte = static_cast<uint8_t>(text[lead]);
    const size_t expected = byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
    if (lead + expected > size) text[lead] = '\0';
    text[capacity - 1] = '\0';
}

bool leap(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
}

void AncsParser::begin(uint32_t uid, uint8_t category, bool metadata) {
    *this = AncsParser{};
    notice_.uid = uid;
    notice_.category = category;
    metadata_ = metadata;
}

void AncsParser::finishAttribute() {
    if (attribute_ == 0) received_ |= 1;
    if (attribute_ == 1) received_ |= 2;
    if (attribute_ == 3) received_ |= 4;
    if (attribute_ == 5) received_ |= 8;
    if (attribute_ == 7) received_ |= 16;
    tupleSize_ = 0;
    consumed_ = 0;
    if (received_ == (metadata_ ? 31 : 7)) {
        trimUtf8(notice_.app, sizeof(notice_.app));
        trimUtf8(notice_.title, sizeof(notice_.title));
        trimUtf8(notice_.message, sizeof(notice_.message));
        trimUtf8(notice_.negativeAction, sizeof(notice_.negativeAction));
        complete_ = true;
    }
}

bool AncsParser::feed(const uint8_t* bytes, size_t size) {
    if (failed_ || complete_) return complete_;
    for (size_t i = 0; i < size && !complete_; ++i) {
        if (headerSize_ < sizeof(header_)) {
            header_[headerSize_++] = bytes[i];
            if (headerSize_ == sizeof(header_)) {
                const uint32_t uid = static_cast<uint32_t>(header_[1]) |
                    (static_cast<uint32_t>(header_[2]) << 8) |
                    (static_cast<uint32_t>(header_[3]) << 16) |
                    (static_cast<uint32_t>(header_[4]) << 24);
                if (header_[0] != 0 || uid != notice_.uid) {
                    failed_ = true;
                    return false;
                }
            }
            continue;
        }
        if (tupleSize_ < sizeof(tuple_)) {
            tuple_[tupleSize_++] = bytes[i];
            if (tupleSize_ == sizeof(tuple_)) {
                attribute_ = tuple_[0];
                length_ = tuple_[1] | (static_cast<uint16_t>(tuple_[2]) << 8);
                consumed_ = 0;
                if (length_ > 4096 || (attribute_ != 0 && attribute_ != 1 && attribute_ != 3 &&
                                      !(metadata_ && (attribute_ == 5 || attribute_ == 7)))) {
                    failed_ = true;
                    return false;
                }
                if (!length_) finishAttribute();
            }
            continue;
        }
        char* destination = attribute_ == 0 ? notice_.app : attribute_ == 1 ? notice_.title :
                            attribute_ == 3 ? notice_.message : attribute_ == 5 ? notice_.date : notice_.negativeAction;
        const size_t capacity = attribute_ == 0 ? sizeof(notice_.app) :
                                attribute_ == 1 ? sizeof(notice_.title) : attribute_ == 3 ? sizeof(notice_.message) :
                                attribute_ == 5 ? sizeof(notice_.date) : sizeof(notice_.negativeAction);
        if (consumed_ < capacity - 1) destination[consumed_] = static_cast<char>(bytes[i]);
        if (++consumed_ == length_) finishAttribute();
    }
    return complete_;
}

void AppNameParser::begin(const char* identifier) {
    *this=AppNameParser{};
    const size_t length=strlen(identifier);
    if(!length || length>=sizeof(expected_)) { failed_=true; return; }
    memcpy(expected_,identifier,length+1);
}

bool AppNameParser::feed(const uint8_t* bytes,size_t size) {
    if(failed_ || complete_) return complete_;
    for(size_t i=0;i<size;++i) {
        const uint8_t value=bytes[i];
        if(stage_==0) { if(value!=1) { failed_=true; return false; } stage_=1; }
        else if(stage_==1) {
            if(!value) {
                if(strcmp(expected_,echoed_)) { failed_=true; return false; }
                stage_=2;
            } else {
                if(position_>=sizeof(echoed_)-1) { failed_=true; return false; }
                echoed_[position_++]=char(value);
            }
        } else if(stage_==2) {
            if(value!=0) { failed_=true; return false; } stage_=3;
        } else if(stage_==3) { length_=value; stage_=4; }
        else if(stage_==4) {
            length_|=uint16_t(value)<<8;
            if(length_>4096) { failed_=true; return false; }
            stage_=5;
            if(!length_) { complete_=true; return true; }
        } else {
            if(consumed_<sizeof(name_)-1) name_[consumed_]=char(value);
            if(++consumed_==length_) { trimUtf8(name_,sizeof(name_)); complete_=true; return true; }
        }
    }
    return false;
}

bool decodeLocalTime(const uint8_t* data, size_t size, int32_t& offsetSeconds) {
    if (size < 2) return false;
    const int zone = static_cast<int8_t>(data[0]);
    const int dst = data[1];
    if (zone < -48 || zone > 56 || (dst != 0 && dst != 2 && dst != 4 && dst != 8)) return false;
    offsetSeconds = (zone + dst) * 15 * 60;
    return true;
}

bool decodeCurrentTime(const uint8_t* data, size_t size, int32_t offsetSeconds,
                       ClockSample& sample) {
    if (size < 10) return false;
    const int year = data[0] | (static_cast<int>(data[1]) << 8);
    const int month = data[2];
    const int day = data[3];
    static const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 ||
        day > monthDays[month - 1] + (month == 2 && leap(year) ? 1 : 0) ||
        data[4] > 23 || data[5] > 59 || data[6] > 59 ||
        offsetSeconds < -12 * 3600 || offsetSeconds > 16 * 3600) return false;
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) days += leap(y) ? 366 : 365;
    for (int m = 1; m < month; ++m) days += monthDays[m - 1] + (m == 2 && leap(year) ? 1 : 0);
    days += day - 1;
    sample.utcSeconds = days * 86400 + data[4] * 3600 + data[5] * 60 + data[6] - offsetSeconds;
    sample.utcOffsetSeconds = offsetSeconds;
    return true;
}

bool isGoogleMaps(const char* id) {
    return strcmp(id, "com.google.Maps") == 0 || strcmp(id, "com.google.maps") == 0;
}

bool isDismissAction(uint8_t category, const char* label) {
    if (category == 1) return false; // Never reject an incoming call via a trash button.
    return !strcmp(label, "Clear") || !strcmp(label, "Dismiss") || !strcmp(label, "Clear notification") ||
        !strcmp(label, "Xóa") || !strcmp(label, "Xoá") || !strcmp(label, "Xóa thông báo") || !strcmp(label, "Xoá thông báo");
}

uint64_t notificationDateKey(const char* date) {
    if(!date || strlen(date)!=15 || date[8]!='T') return 0;
    uint64_t key=0;
    for(int i=0;i<15;++i) if(i!=8) {
        if(date[i]<'0' || date[i]>'9') return 0;
        key=key*10+unsigned(date[i]-'0');
    }
    auto pair=[date](int index) { return (date[index]-'0')*10+date[index+1]-'0'; };
    const int year=pair(0)*100+pair(2), month=pair(4), day=pair(6);
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(year<2000 || year>2099 || month<1 || month>12 || day<1 ||
       day>days[month-1]+(month==2 && leap(year) ? 1 : 0) || pair(9)>23 || pair(11)>59 || pair(13)>59) return 0;
    return key;
}

bool decodeMusicUpdate(const uint8_t* data, size_t size, MusicUpdate& update) {
    if (size < 3) return false;
    MusicField field;
    if (data[0] == 0 && data[1] <= 2) field = static_cast<MusicField>(data[1]);
    else if (data[0] == 2 && data[1] == 0) field = MusicField::Artist;
    else if (data[0] == 2 && data[1] == 2) field = MusicField::Title;
    else return false;
    update = MusicUpdate{};
    update.field = field;
    update.truncated = (data[2] & 1) != 0;
    const size_t length = size - 3 < sizeof(update.text) - 1 ? size - 3 : sizeof(update.text) - 1;
    memcpy(update.text, data + 3, length);
    trimUtf8(update.text, sizeof(update.text));
    return true;
}

uint16_t decodeMusicCommands(const uint8_t* data, size_t size) {
    uint16_t commands = 0;
    for (size_t i = 0; i < size; ++i) if (data[i] < 14) commands |= uint16_t(1) << data[i];
    return commands;
}
}
