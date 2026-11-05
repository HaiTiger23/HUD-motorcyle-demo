#pragma once
#include <stdint.h>
#include <stddef.h>
#include <algorithm>

namespace hudvoice {
// Standard IMA ADPCM, independently decodable blocks. Low nibble first.
static constexpr int STEPS[] = {7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static constexpr int INDEX[] = {-1,-1,-1,-1,2,4,6,8};
inline uint8_t encode(int sample, int& predictor, int& index) {
    const int step=STEPS[index];
    int difference=sample-predictor, delta=step>>3;
    uint8_t code=0;
    if(difference<0) { code=8; difference=-difference; }
    if(difference>=step) { code|=4; difference-=step; delta+=step; }
    if(difference>=(step>>1)) { code|=2; difference-=step>>1; delta+=step>>1; }
    if(difference>=(step>>2)) { code|=1; delta+=step>>2; }
    predictor=std::max(-32768,std::min(32767,predictor+((code&8) ? -delta : delta)));
    index=std::max(0,std::min(88,index+INDEX[code&7]));
    return code;
}
inline int16_t decode(uint8_t code, int& predictor, int& index) {
    const int step=STEPS[index];
    int delta=(step>>3)+((code&4) ? step : 0)+((code&2) ? step>>1 : 0)+((code&1) ? step>>2 : 0);
    predictor=std::max(-32768,std::min(32767,predictor+((code&8) ? -delta : delta)));
    index=std::max(0,std::min(88,index+INDEX[code&7]));
    return predictor;
}
}
