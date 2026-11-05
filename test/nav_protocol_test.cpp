#include <nav_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
using Bytes = std::vector<uint8_t>;
void word(Bytes& bytes, uint32_t value) { for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (8 * i))); }
Bytes start(const Bytes& jpeg, uint32_t id = 123) {
    Bytes packet{1}; word(packet, id); word(packet, jpeg.size()); word(packet, hudnav::crc32(jpeg.data(), jpeg.size())); return packet;
}
int main() {
    const uint8_t standard[] = "123456789";
    assert(hudnav::crc32(standard, 9) == 0xcbf43926);
    // Header fixture exercises the transport/dimension gate, not JPEG decoding.
    Bytes jpeg{0xff,0xd8,0xff,0xc0,0,8,8,0,200,1,44,3};
    jpeg.resize(1500, 0x5a);
    assert(hudnav::validDimensions(jpeg.data(), jpeg.size()));
    jpeg[8] = 150;
    assert(!hudnav::validDimensions(jpeg.data(), jpeg.size()));
    jpeg[8] = 200;
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
    puts("Navigation frame fragmentation, CRC, bounds and JPEG dimension checks passed");
}
