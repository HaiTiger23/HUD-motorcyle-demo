#include <nav_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <cstring>
using Bytes = std::vector<uint8_t>;
void word(Bytes& bytes, uint32_t value) { for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (8 * i))); }
Bytes start(const Bytes& jpeg, uint32_t id = 123) {
    Bytes packet{1}; word(packet, id); word(packet, jpeg.size()); word(packet, hudnav::crc32(jpeg.data(), jpeg.size())); return packet;
}
int main() {
    const uint8_t standard[] = "123456789";
    assert(hudnav::crc32(standard, 9) == 0xcbf43926);
    // Header fixture exercises the transport/dimension gate, not JPEG decoding.
    Bytes jpeg{0xff,0xd8,0xff,0xc0,0,8,8,0,240,1,64,3};
    jpeg.resize(1500, 0x5a);
    assert(hudnav::validDimensions(jpeg.data(), jpeg.size()));
    jpeg[8] = 150;
    assert(!hudnav::validDimensions(jpeg.data(), jpeg.size()));
    jpeg[8] = 240;
    uint8_t buffer[2048];
    for (size_t chunk : {size_t(1), size_t(11), size_t(55), size_t(235)}) {
        hudnav::Receiver rx(buffer, sizeof(buffer));
        auto header = start(jpeg);
        assert(rx.feed(header.data(), header.size()) == hudnav::Result::Partial);
        for (size_t offset = 0; offset < jpeg.size(); offset += chunk) {
            Bytes packet{2}; word(packet, 123); word(packet, offset);
            packet.insert(packet.end(), jpeg.begin() + offset, jpeg.begin() + std::min(offset + chunk, jpeg.size()));
            assert(rx.feed(packet.data(), packet.size()) == hudnav::Result::Partial);
        }
        const uint8_t end[] = {3,123,0,0,0};
        assert(rx.feed(end, sizeof(end)) == hudnav::Result::Complete);
        assert(rx.size() == jpeg.size() && std::equal(jpeg.begin(), jpeg.end(), buffer));
        assert(rx.feed(end, sizeof(end)) == hudnav::Result::Rejected);
    }
    hudnav::Receiver rx(buffer, sizeof(buffer));
    auto header = start(jpeg);
    rx.feed(header.data(), header.size());
    const uint8_t reordered[] = {2,123,0,0,0,1,0,0,0,42};
    assert(rx.feed(reordered, sizeof(reordered)) == hudnav::Result::Rejected);
    header[5] = 255; header[6] = 255;
    assert(rx.feed(header.data(), header.size()) == hudnav::Result::Rejected);
    jpeg[10] = 45;
    assert(!hudnav::validDimensions(jpeg.data(), jpeg.size()));
    for (size_t size = 0; size < 12; ++size) assert(!hudnav::validDimensions(jpeg.data(), size));
    jpeg[10] = 44;
    header = start(jpeg); rx.feed(header.data(), header.size());
    Bytes corrupt{2}; word(corrupt, 123); word(corrupt, 0);
    corrupt.insert(corrupt.end(), jpeg.begin(), jpeg.end()); corrupt.back() ^= 1;
    rx.feed(corrupt.data(), corrupt.size());
    const uint8_t end[] = {3,123,0,0,0};
    assert(rx.feed(end, sizeof(end)) == hudnav::Result::Rejected);
    hudnav::TripSummary summary;
    Bytes trip{1,0x59,1,0,0,80,0,0,0,0x1c,2,2,'R',0xe1,0xba,0xbd};
    assert(hudnav::decodeTrip(trip.data(), trip.size(), summary));
    assert(summary.remainingMetres == 345 && summary.turnMetres == 80 && summary.etaMinute == 540);
    assert(std::strcmp(summary.instruction, "Rẽ") == 0 && !summary.simulating && summary.followingManeuver == 255);
    trip.insert(trip.end(), {0,1,1,30,0});
    assert(hudnav::decodeTrip(trip.data(), trip.size(), summary));
    assert(summary.simulating && summary.followingManeuver == 1 && summary.followingMetres == 30);
    assert(std::strcmp(summary.instruction, "Rẽ") == 0);
    trip[trip.size() - 2] = 101;
    assert(hudnav::decodeTrip(trip.data(), trip.size(), summary) && summary.followingManeuver == 255);
    trip.pop_back();
    assert(hudnav::decodeTrip(trip.data(), trip.size(), summary) && !summary.simulating && summary.followingManeuver == 255);
    assert(!hudnav::decodeTrip(trip.data(), 11, summary));
    hudnav::MapMotion motion;
    const Bytes movement{7,0x78,0x56,0x34,0x12,0x2c,1,3};
    assert(hudnav::decodeMapMotion(movement.data(), movement.size(), motion));
    assert(motion.frame == 0x12345678 && motion.width() == 344 && motion.height() == 264);
    assert(motion.arrow && motion.light && motion.offset(100) == 3 && motion.offset(5000) == 12);
    assert(!hudnav::decodeMapMotion(movement.data(), movement.size() - 1, motion));
    const Bytes badMotion{7,0,0,0,0,0,0,3};
    assert(!hudnav::decodeMapMotion(badMotion.data(), badMotion.size(), motion));
    const Bytes fastMotion{7,2,0,0,0,0xf4,1,1,48,1,0,0,0,20,0};
    assert(hudnav::decodeMapMotion(fastMotion.data(), fastMotion.size(), motion));
    assert(motion.frame == 2 && motion.previousFrame == 1 && motion.advance == 20);
    assert(motion.width() == 320 && motion.height() == 336);
    assert(motion.offset(380) == 19 && motion.offset(800) == 40 && motion.offset(1200) == 48);
    // Match the last visible position when the next frame is 20 pixels farther along.
    const int matchedOffset = motion.offset(800) - motion.advance;
    const int correction = matchedOffset - motion.offset(350);
    assert(motion.offset(350, correction, 0) == matchedOffset);
    assert(motion.offset(380, correction, 30) > matchedOffset);
    assert(motion.offset(350, -30, 0) == -12);
    assert(motion.offset(700, -30, 350) == 35);
    assert(motion.offset(350, -100, 0) == -48);
    auto invalidMargin = fastMotion;
    invalidMargin[8] = 65;
    assert(!hudnav::decodeMapMotion(invalidMargin.data(), invalidMargin.size(), motion));
    puts("Navigation frame fragmentation, CRC, bounds and JPEG dimension checks passed");
}
