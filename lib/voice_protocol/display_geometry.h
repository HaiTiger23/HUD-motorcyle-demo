#pragma once
#include <stdint.h>
#include <math.h>
namespace huddisplay {
struct Point { uint16_t x,y; };
inline Point touch(uint16_t rawX,uint16_t rawY,uint8_t rotation) {
    if(rawX>239) rawX=239;
    if(rawY>319) rawY=319;
    switch(rotation&3) {
    case 1:return {uint16_t(239-rawX),uint16_t(319-rawY)};
    case 2:return {uint16_t(319-rawY),rawX};
    case 3:return {rawX,rawY};
    default:return {rawY,uint16_t(239-rawX)};
    }
}
inline uint8_t duty(uint8_t brightness,bool awake) { return awake ? uint8_t((unsigned(brightness)*255+50)/100) : 0; }
inline uint8_t speakerGain(uint8_t volume) {
    if(!volume) return 0;
    // ES8311 0xBF is 0 dB; never use its +32 dB boost range.
    if(volume>100) volume=100;
    return uint8_t(191+int(roundf(40*log10f(float(volume)/100))));
}
}
