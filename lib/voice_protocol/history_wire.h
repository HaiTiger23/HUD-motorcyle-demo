#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace hudvoice {
struct HistoryHeader { uint32_t token; uint8_t index,count; };
inline bool readHistoryPacket(const uint8_t* bytes,size_t size,HistoryHeader& out) {
    if(!bytes || size<7 || size>198 || bytes[0]!=0x54) return false;
    const uint32_t token=uint32_t(bytes[1]) | uint32_t(bytes[2])<<8 | uint32_t(bytes[3])<<16 | uint32_t(bytes[4])<<24;
    const uint8_t index=bytes[5],count=bytes[6];
    if(!token || index>=5 || count>5 || memchr(bytes+7,0,size-7) ||
       (!count && (index || size!=7)) || (count && index>=count)) return false;
    out={token,index,count}; return true;
}
}
