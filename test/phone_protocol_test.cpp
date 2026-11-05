#include "phone_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;

void attribute(Bytes& bytes, uint8_t id, const std::string& value) {
    bytes.push_back(id);
    bytes.push_back(value.size() & 255);
    bytes.push_back(value.size() >> 8);
    bytes.insert(bytes.end(), value.begin(), value.end());
}

Bytes response(uint32_t uid, const std::string& title, const std::string& body) {
    Bytes bytes{0, uint8_t(uid), uint8_t(uid >> 8), uint8_t(uid >> 16), uint8_t(uid >> 24)};
    attribute(bytes, 0, "com.google.Maps");
    attribute(bytes, 1, title);
    attribute(bytes, 3, body);
    return bytes;
}

int main() {
    const std::string appId="com.apple.reminders";
    Bytes appName{1}; appName.insert(appName.end(),appId.begin(),appId.end()); appName.push_back(0);
    attribute(appName,0,u8"Lời nhắc");
    for(size_t chunk=1;chunk<=appName.size();++chunk) {
        phone::AppNameParser parser; parser.begin(appId.c_str());
        bool done=false;
        for(size_t offset=0;offset<appName.size();offset+=chunk)
            done=parser.feed(appName.data()+offset,std::min(chunk,appName.size()-offset));
        assert(done && !parser.failed()); assert(!strcmp(parser.name(),u8"Lời nhắc"));
    }
    phone::AppNameParser wrongApp; wrongApp.begin("com.example.other");
    wrongApp.feed(appName.data(),appName.size()); assert(wrongApp.failed());
    Bytes longApp{1}; longApp.insert(longApp.end(),appId.begin(),appId.end()); longApp.push_back(0);
    attribute(longApp,0,std::string(94,'a')+u8"ế");
    phone::AppNameParser cutApp; cutApp.begin(appId.c_str());
    assert(cutApp.feed(longApp.data(),longApp.size())); assert(strlen(cutApp.name())==94);
    assert(phone::notificationDateKey("20261031T125900")>phone::notificationDateKey("20261031T120000"));
    assert(phone::notificationDateKey("20261031T120000")>phone::notificationDateKey("20261030T235900"));
    assert(phone::notificationDateKey("20240229T120000")!=0);
    assert(phone::notificationDateKey("20230229T120000")==0);
    assert(phone::notificationDateKey("20261031T245900")==0);
    assert(phone::notificationDateKey("")==0);
    auto detailed=response(42,"Title","Message");
    attribute(detailed,5,"20261031T074500"); attribute(detailed,7,"Clear");
    for(size_t chunk=1;chunk<=detailed.size();++chunk) {
        phone::AncsParser parser; parser.begin(42,4,true);
        for(size_t offset=0;offset<detailed.size();offset+=chunk)
            parser.feed(detailed.data()+offset,std::min(chunk,detailed.size()-offset));
        assert(parser.complete() && !parser.failed());
        assert(!strcmp(parser.notice().date,"20261031T074500"));
        assert(!strcmp(parser.notice().negativeAction,"Clear"));
    }
    assert(phone::isDismissAction(4,"Clear"));
    assert(phone::isDismissAction(4,u8"Xóa"));
    assert(!phone::isDismissAction(1,"Clear"));
    assert(!phone::isDismissAction(4,"Decline"));
    assert(!phone::isDismissAction(4,"Delete conversation"));
    assert(!phone::isDismissAction(4,""));
    const auto message = response(0xFEDC1234, u8"Rẽ trái", u8"Sau 200 m, vào đường Nguyễn Huệ");
    for (size_t chunk = 1; chunk <= message.size(); ++chunk) {
        phone::AncsParser parser;
        parser.begin(0xFEDC1234, 10);
        for (size_t offset = 0; offset < message.size(); offset += chunk) {
            const size_t length = message.size() - offset < chunk ? message.size() - offset : chunk;
            parser.feed(message.data() + offset, length);
        }
        assert(parser.complete());
        assert(!parser.failed());
        assert(parser.notice().category == 10);
        assert(strcmp(parser.notice().title, u8"Rẽ trái") == 0);
        assert(strcmp(parser.notice().message, u8"Sau 200 m, vào đường Nguyễn Huệ") == 0);
        assert(phone::isGoogleMaps(parser.notice().app));
    }

    phone::AncsParser wrongUid;
    wrongUid.begin(1, 0);
    wrongUid.feed(message.data(), message.size());
    assert(wrongUid.failed() && !wrongUid.complete());

    const auto empty = response(7, "", "");
    phone::AncsParser emptyParser;
    emptyParser.begin(7, 0);
    assert(emptyParser.feed(empty.data(), empty.size()));
    assert(emptyParser.notice().title[0] == '\0');

    const auto longText = response(9, std::string(158, 'a') + u8"ế", std::string(1800, 'b'));
    phone::AncsParser bounded;
    bounded.begin(9, 0);
    assert(bounded.feed(longText.data(), longText.size()));
    assert(strlen(bounded.notice().title) == 158); // no partial UTF-8 character
    assert(strlen(bounded.notice().message) == 511);

    phone::AncsParser truncated;
    truncated.begin(0xFEDC1234, 0);
    truncated.feed(message.data(), message.size() - 1);
    assert(!truncated.complete());

    const uint8_t malicious[] = {0, 0, 0, 0, 0, 0, 255, 255};
    phone::AncsParser invalidLength;
    invalidLength.begin(0, 0);
    invalidLength.feed(malicious, sizeof(malicious));
    assert(invalidLength.failed());

    int32_t zone = 0;
    const uint8_t vietnam[] = {28, 0};
    const uint8_t eastern[] = {0xEC, 0};
    const uint8_t summer[] = {28, 4};
    const uint8_t unknown[] = {0x80, 255};
    assert(phone::decodeLocalTime(vietnam, 2, zone) && zone == 7 * 3600);
    assert(phone::decodeLocalTime(eastern, 2, zone) && zone == -5 * 3600);
    assert(phone::decodeLocalTime(summer, 2, zone) && zone == 8 * 3600);
    assert(!phone::decodeLocalTime(unknown, 2, zone));

    uint8_t currentTime[] = {0xE8, 0x07, 2, 29, 7, 0, 0, 4, 0, 1};
    phone::ClockSample sample;
    assert(phone::decodeCurrentTime(currentTime, 10, 7 * 3600, sample));
    assert(sample.utcSeconds == 1709164800LL);
    assert(sample.utcOffsetSeconds == 7 * 3600);
    currentTime[0] = 0xE7; // 2023 was not a leap year.
    assert(!phone::decodeCurrentTime(currentTime, 10, 7 * 3600, sample));
    assert(!phone::decodeCurrentTime(currentTime, 9, 7 * 3600, sample));
    assert(!phone::isGoogleMaps("com.example.anotherMaps"));
    phone::MusicUpdate music;
    Bytes track{2, 2, 0};
    const std::string song = u8"Ngày mai em đi";
    track.insert(track.end(), song.begin(), song.end());
    assert(phone::decodeMusicUpdate(track.data(), track.size(), music));
    assert(music.field == phone::MusicField::Title && !music.truncated);
    assert(strcmp(music.text, song.c_str()) == 0);
    track[2] = 1;
    assert(phone::decodeMusicUpdate(track.data(), track.size(), music) && music.truncated);
    assert(!phone::decodeMusicUpdate(track.data(), 2, music));
    const uint8_t ignored[] = {1, 0, 0};
    assert(!phone::decodeMusicUpdate(ignored, sizeof(ignored), music));
    const uint8_t emptyTitle[] = {2, 2, 0};
    assert(phone::decodeMusicUpdate(emptyTitle, sizeof(emptyTitle), music) && !music.text[0]);
    Bytes longTrack{2, 0, 0};
    const std::string longArtist = std::string(158, 'a') + u8"ế";
    longTrack.insert(longTrack.end(), longArtist.begin(), longArtist.end());
    assert(phone::decodeMusicUpdate(longTrack.data(), longTrack.size(), music));
    assert(strlen(music.text) == 158);
    const uint8_t supported[] = {0, 1, 2, 3, 4, 5, 6, 255};
    assert(phone::decodeMusicCommands(supported, sizeof(supported)) == 127);
    assert(phone::decodeMusicCommands(nullptr, 0) == 0);
    puts("ANCS, bounded UTF-8, CTS and AMS protocol tests passed");
}
