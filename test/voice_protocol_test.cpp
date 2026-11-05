#include <voice_protocol.h>
#include <history_wire.h>
#include <assert.h>
#include <stdio.h>
#include <ai_gesture.h>
#include <ai_typewriter.h>
#include <display_geometry.h>
#include <string.h>
int main() {
    hudvoice::HistoryHeader history{};
    const uint8_t emptyHistory[]={0x54,1,0,0,0,0,0};
    assert(hudvoice::readHistoryPacket(emptyHistory,sizeof(emptyHistory),history));
    assert(history.token==1 && history.index==0 && history.count==0);
    uint8_t fullHistory[199]; memset(fullHistory,'a',sizeof(fullHistory));
    memcpy(fullHistory,emptyHistory,7); fullHistory[5]=4; fullHistory[6]=5;
    assert(hudvoice::readHistoryPacket(fullHistory,198,history));
    assert(!hudvoice::readHistoryPacket(fullHistory,199,history));
    fullHistory[6]=4; assert(!hudvoice::readHistoryPacket(fullHistory,198,history));
    fullHistory[6]=6; assert(!hudvoice::readHistoryPacket(fullHistory,198,history));
    fullHistory[6]=5; fullHistory[10]=0; assert(!hudvoice::readHistoryPacket(fullHistory,198,history));
    fullHistory[1]=0; assert(!hudvoice::readHistoryPacket(fullHistory,7,history));
    assert(!hudvoice::readHistoryPacket(nullptr,0,history));
    puts("HUD history: empty/fifth item, bounded payload, stale-shape indices, invalid count and NUL rejection passed");
    int predictor=0,index=0;
    assert(hudvoice::encode(100,predictor,index)==7 && predictor==11 && index==8);
    assert(hudvoice::encode(-100,predictor,index)==15 && predictor==-19 && index==16);
    assert(hudvoice::encode(100,predictor,index)==7 && predictor==44 && index==24);
    predictor=0; index=0;
    assert(hudvoice::decode(7,predictor,index)==11);
    assert(hudvoice::decode(15,predictor,index)==-19);
    assert(hudvoice::decode(7,predictor,index)==44);
    predictor=32767; index=88;
    assert(hudvoice::decode(7,predictor,index)==32767 && index==88);
    predictor=-32768; index=88;
    assert(hudvoice::decode(15,predictor,index)==-32768 && index==88);
    puts("IMA ADPCM golden samples, nibble order and saturation passed");
    predictor=100; index=0;
    assert(hudvoice::encode(-100,predictor,index)==15 && predictor==89);
    assert(hudvoice::encode(100,predictor,index)==2 && predictor==99);
    using hudvoice::Gesture;
    hudvoice::AiGesture gesture;
    assert(gesture.update(true,150,220,0,false,true,false)==Gesture::None);
    assert(gesture.update(true,152,165,100,false,true,false)==Gesture::Open);
    assert(gesture.consumes());
    assert(gesture.update(false,152,165,110,true,true,false)==Gesture::None);
    assert(gesture.update(true,150,100,200,true,true,false)==Gesture::None);
    assert(gesture.update(true,150,100,549,true,true,false)==Gesture::None);
    assert(gesture.update(true,150,100,550,true,true,false)==Gesture::Begin);
    assert(gesture.update(true,150,100,100000,true,false,false)==Gesture::None);
    assert(gesture.update(false,150,100,100001,true,false,false)==Gesture::Send);
    gesture.update(true,150,90,101000,true,true,false);
    assert(gesture.update(true,150,90,101350,true,true,false)==Gesture::Begin);
    assert(gesture.update(true,150,135,101400,true,false,false)==Gesture::Cancel);
    assert(gesture.update(false,150,135,101450,true,true,false)==Gesture::None);
    gesture.update(true,150,150,102000,true,true,false);
    gesture.update(true,150,120,102100,true,true,false);
    assert(gesture.update(true,150,120,103000,true,true,false)==Gesture::None);
    gesture.update(false,150,120,103050,true,true,false);
    gesture.update(true,150,20,104000,true,true,false);
    assert(gesture.update(false,150,80,104100,true,true,false)==Gesture::Close);
    gesture.update(true,150,100,105000,true,true,true);
    assert(gesture.update(true,150,100,106000,true,true,true)==Gesture::None);
    gesture.update(false,150,100,106050,true,true,true);
    // Delete confirmation and pairing modals must receive ordinary contacts.
    for(bool aiPage: {false,true}) {
        hudvoice::AiGesture modal;
        for(int tap=0;tap<3;++tap) {
            const uint32_t at=uint32_t(tap*1000);
            assert(modal.update(true,180,160,at,aiPage,true,true)==Gesture::None);
            assert(!modal.consumes());
            assert(modal.update(true,180,160,at+400,aiPage,true,true)==Gesture::None);
            assert(!modal.consumes());
            assert(modal.update(false,180,160,at+450,aiPage,true,true)==Gesture::None);
            assert(!modal.consumes());
        }
        // Hiding a modal mid-contact must not turn its tap into AI recording.
        modal.update(true,150,100,4000,aiPage,true,true);
        assert(modal.update(true,150,100,4500,true,true,false)==Gesture::None);
        assert(!modal.consumes());
        modal.update(false,150,100,4600,true,true,false);
        modal.update(true,150,100,5000,true,true,false);
        assert(modal.update(true,150,100,5350,true,true,false)==Gesture::Begin);
        assert(modal.consumes());
        // Interrupt an already-owned recording without leaking a button press.
        assert(modal.update(true,150,100,5400,true,true,true)==Gesture::Cancel);
        assert(modal.consumes());
        assert(modal.update(true,150,100,5500,true,true,true)==Gesture::None);
        assert(modal.consumes());
        modal.update(false,150,100,5600,true,true,true);
        assert(!modal.consumes());
    }
    puts("Modal touch regression: confirm/cancel pass through; recording interruption and fresh holds passed");
    puts("AI gestures: bottom drawer, hold/release, long hold, drag cancel, scroll, close and wake suppression passed");
    const char* vietnamese="Rẽ trái 🙂";
    size_t shown=0;
    const size_t boundaries[]={1,4,5,6,7,9,10,11,15};
    for(size_t boundary:boundaries) {
        shown=hudvoice::revealUtf8(vietnamese,strlen(vietnamese),shown,1);
        assert(shown==boundary);
    }
    assert(hudvoice::revealUtf8(vietnamese,strlen(vietnamese),shown,100)==strlen(vietnamese));
    assert(hudvoice::revealUtf8("",0,0,1)==0);
    assert(hudvoice::revealUtf8("\xe1\xba",2,0,1)==0);
    puts("AI typewriter: Vietnamese, emoji, end-of-text and incomplete UTF-8 passed");
    const auto p0=huddisplay::touch(0,0,0),p1=huddisplay::touch(0,0,1),p2=huddisplay::touch(0,0,2),p3=huddisplay::touch(0,0,3);
    assert(p0.x==0 && p0.y==239); assert(p1.x==239 && p1.y==319);
    assert(p2.x==319 && p2.y==0); assert(p3.x==0 && p3.y==0);
    for(int rotation=0;rotation<4;++rotation) {
        const auto point=huddisplay::touch(239,319,rotation);
        assert(point.x<(rotation%2 ? 240 : 320)); assert(point.y<(rotation%2 ? 320 : 240));
    }
    assert(huddisplay::duty(100,true)==255 && huddisplay::duty(5,true)==13 && huddisplay::duty(100,false)==0);
    assert(huddisplay::speakerGain(0)==0 && huddisplay::speakerGain(100)==191);
    for(int volume=1;volume<=100;++volume) assert(huddisplay::speakerGain(volume)<=191 && huddisplay::speakerGain(volume)>=huddisplay::speakerGain(volume-1));
    hudvoice::AiGesture portrait;
    portrait.update(true,120,305,0,false,true,false,320);
    assert(portrait.update(true,120,250,100,false,true,false,320)==Gesture::Open);
    portrait.update(false,120,250,110,true,true,false,320);
    portrait.update(true,120,260,200,true,true,false,320);
    assert(portrait.update(true,120,260,550,true,true,false,320)==Gesture::Begin);
    assert(portrait.update(true,120,300,600,true,true,false,320)==Gesture::Cancel);
    puts("Display: four touch rotations, coordinate bounds, PWM and portrait AI gestures passed");
}
