#pragma once
#include <stddef.h>

namespace hudvoice {
// Advance by Unicode code points rather than bytes so Vietnamese letters and
// four-byte emoji never appear as a partially decoded UTF-8 sequence.
inline size_t revealUtf8(const char* text, size_t length, size_t offset, size_t characters) {
    if(offset>length) offset=length;
    while(offset<length && characters--) {
        const unsigned char first=static_cast<unsigned char>(text[offset]);
        size_t width=first<0x80 ? 1 : first>=0xC2 && first<=0xDF ? 2 :
            first>=0xE0 && first<=0xEF ? 3 : first>=0xF0 && first<=0xF4 ? 4 : 1;
        if(width>length-offset) return offset;
        for(size_t i=1;i<width;++i) {
            if((static_cast<unsigned char>(text[offset+i])&0xC0)!=0x80) { width=1; break; }
        }
        offset+=width;
    }
    return offset;
}
}
