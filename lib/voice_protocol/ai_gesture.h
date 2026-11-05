#pragma once
#include <stdint.h>
#include <stdlib.h>
namespace hudvoice {
enum class Gesture { None, Open, Close, Begin, Send, Cancel };
class AiGesture {
    enum class Stage { Idle, Press, Drag, Listening, Consumed, Suppressed } stage=Stage::Idle;
    int startX=0,startY=0;
    uint32_t started=0;
public:
    bool consumes() const { return stage==Stage::Listening || stage==Stage::Consumed; }
    Gesture update(bool down,int x,int y,uint32_t now,bool ai,bool canRecord,bool blocked,int height=240) {
        if(blocked || (!ai && stage==Stage::Listening)) {
            const bool listening=stage==Stage::Listening;
            // A modal disables AI gestures; its own buttons still need touch
            // events. Only swallow a contact already claimed by an AI gesture.
            const bool owned=consumes();
            stage=down ? (owned ? Stage::Consumed : Stage::Suppressed) : Stage::Idle;
            return listening ? Gesture::Cancel : Gesture::None;
        }
        if(stage==Stage::Suppressed) {
            if(!down) stage=Stage::Idle;
            return Gesture::None;
        }
        if(stage==Stage::Idle) {
            if(down) { stage=Stage::Press; startX=x; startY=y; started=now; }
            return Gesture::None;
        }
        const int dx=x-startX,dy=y-startY;
        if(stage==Stage::Listening) {
            const int cancelDistance=startY>height-41 ? (height-1-startY>10 ? height-1-startY : 10) : 40;
            if(dy>=cancelDistance) { stage=down ? Stage::Consumed : Stage::Idle; return Gesture::Cancel; }
            if(!down) { stage=Stage::Idle; return Gesture::Send; }
            return Gesture::None;
        }
        if(stage!=Stage::Consumed) {
            if(!ai && startY>=height-40 && dy<=-45 && abs(dx)<-dy) {
                stage=down ? Stage::Consumed : Stage::Idle; return Gesture::Open;
            }
            if(ai && startY<48 && dy>=45 && abs(dx)<dy) {
                stage=down ? Stage::Consumed : Stage::Idle; return Gesture::Close;
            }
            if(abs(dx)>10 || abs(dy)>10) stage=Stage::Drag;
            if(stage==Stage::Press && down && ai && startY>=48 && canRecord && now-started>=350) {
                stage=Stage::Listening; return Gesture::Begin;
            }
        }
        if(!down) stage=Stage::Idle;
        return Gesture::None;
    }
};
}
