#include "ai_voice.h"
#include "hud_settings.h"
#include <Arduino.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <driver/i2s.h>
#include <voice_protocol.h>
#include <history_wire.h>
#include <atomic>
#include <math.h>
#include <display_geometry.h>

namespace {
NimBLEServer* server;
NimBLECharacteristic* channel;
SemaphoreHandle_t guard;
AiVoiceView view{};
AiDestinationView destination{};
AiHistoryView history{};
std::atomic<uint32_t> historyWaitingAt{0};
std::atomic<uint32_t> destinationAccepted{0},destinationStale{0};
std::atomic<uint8_t> destinationLastPhase{0};
std::atomic<bool> held{false}, canceled{false}, active{false};
std::atomic<bool> recording{false};
std::atomic<uint32_t> session{0}, phoneReady{0};
std::atomic<uint8_t> voicePurpose{0};
bool microphone=false;
std::atomic<bool> tonePending{false},toneBusy{false};
struct PlaybackBlock { uint16_t size; uint8_t bytes[172]; };
struct PlaybackAck { uint32_t id; uint16_t peer; uint8_t phase; uint16_t consumed; };
QueueHandle_t playbackQueue,playbackAcks;
std::atomic<bool> playing{false},playbackEnded{false};
std::atomic<bool> playbackWorkerBusy{false};
std::atomic<uint32_t> playbackId{0},playbackSamples{0},playbackExpected{0},playbackAt{0};
std::atomic<uint16_t> playbackPeer{0},playbackReceived{0};
std::atomic<uint32_t> playbackErrors{0},playbackCompleted{0};
std::atomic<uint32_t> playbackWaits{0},playbackDuration{0},playbackExpectedMs{0};
std::atomic<uint8_t> playbackLastError{0};
void ackPlayback(uint32_t id,uint16_t peer,uint8_t phase,uint16_t consumed) {
    PlaybackAck ack{id,peer,phase,consumed};
    if(playbackAcks) xQueueSend(playbackAcks,&ack,0);
}
QueueHandle_t i2sEvents;
std::atomic<uint32_t> samplesSent{0}, txErrors{0}, recordings{0};
std::atomic<int> peak{0};
uint16_t textSequence[3]{};
std::atomic<uint32_t> processingAt{0};
QueueHandle_t toolRequests;
std::atomic<uint32_t> toolEpoch{0};
struct ToolReply {
    AiToolRequest request{};
    bool success=false,pending=false;
    uint16_t offset=0,sequence=0;
    uint32_t started=0;
    char text[3072]{};
} toolReply;
bool writeCodec(uint8_t reg,uint8_t value) {
    Wire.beginTransmission(0x18); Wire.write(reg); Wire.write(value);
    return Wire.endTransmission()==0;
}
void state(const char* text) {
    xSemaphoreTake(guard,portMAX_DELAY);
    strlcpy(view.status,text,sizeof(view.status)); view.busy=active.load(); view.recording=recording.load();
    view.session=session.load(); ++view.revision;
    xSemaphoreGive(guard);
}
uint32_t word(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
void putWord(uint8_t* p,uint32_t n) { for(int i=0;i<4;++i) p[i]=n>>(8*i); }
bool notify(const uint8_t* bytes,size_t size,uint32_t id) {
    for(int retry=0;retry<20;++retry) {
        if(canceled || session!=id || !server || server->getConnectedCount()!=1) return false;
        const auto peer=server->getPeerInfo(uint8_t(0));
        if(!peer.isEncrypted()) return false;
        if(channel->notify(bytes,size,peer.getConnHandle())) return true;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    ++txErrors; return false;
}
class Input : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch,NimBLEConnInfo& peer) override {
        if(!peer.isEncrypted()) return;
        const auto bytes=ch->getValue(); const auto* b=bytes.data(); const size_t n=bytes.size();
        if(n>=5 && b[0]>=0x40 && b[0]<=0x43) {
            const uint32_t id=word(b+1);
            if(b[0]==0x43) { if(id==playbackId) { playing=false; digitalWrite(1,HIGH); } return; }
            if(b[0]==0x40) {
                const uint32_t total=n==12 ? word(b+8) : 0;
                if(n!=12 || !id || b[5]!=0x40 || b[6]!=0x1f || b[7]!=1 || !total || total>720000 || held || active || toneBusy || playing || playbackWorkerBusy || !playbackQueue) {
                    ackPlayback(id,peer.getConnHandle(),3,0); return;
                }
                xQueueReset(playbackQueue);
                playbackId=id; playbackPeer=peer.getConnHandle(); playbackExpected=total;
                playbackSamples=0; playbackReceived=0; playbackEnded=false; playbackAt=millis(); playing=true;
                ackPlayback(id,peer.getConnHandle(),0,0); return;
            }
            if(!playing || id!=playbackId) return;
            if(b[0]==0x41) {
                const size_t count=n>=12 ? b[10]|uint16_t(b[11])<<8 : 0;
                const uint16_t sequence=n>=7 ? b[5]|uint16_t(b[6])<<8 : 65535;
                bool valid=count && count<=320 && n==12+count/2 && b[9]<=88 && sequence==playbackReceived &&
                    playbackSamples+count<=playbackExpected && !playbackEnded;
                PlaybackBlock block{};
                if(valid) { block.size=n; memcpy(block.bytes,b,n); valid=xQueueSend(playbackQueue,&block,0)==pdTRUE; }
                if(!valid) { playing=false; ++playbackErrors; playbackLastError=1; ackPlayback(id,peer.getConnHandle(),3,0); }
                else { ++playbackReceived; playbackSamples+=count; playbackAt=millis(); }
            } else if(b[0]==0x42) {
                if(n!=7 || (b[5]|uint16_t(b[6])<<8)!=playbackReceived || playbackSamples!=playbackExpected) {
                    playing=false; ++playbackErrors; ackPlayback(id,peer.getConnHandle(),3,0);
                } else playbackEnded=true;
            }
            return;
        }
        if(n==8 && b[0]==0x30) {
            AiToolRequest request{word(b+1),toolEpoch.load(),peer.getConnHandle(),b[5],uint16_t(b[6]|uint16_t(b[7])<<8)};
            if(toolRequests) xQueueSend(toolRequests,&request,0);
            return;
        }
        if(n==2 && b[0]==0x56 && b[1]<=1) {
            xSemaphoreTake(guard,portMAX_DELAY); history.navigating=b[1]; ++history.revision; xSemaphoreGive(guard); return;
        }
        if(n && b[0]==0x54) {
            hudvoice::HistoryHeader header{};
            if(!hudvoice::readHistoryPacket(b,n,header)) return;
            const uint32_t token=header.token; const uint8_t index=header.index,total=header.count;
            xSemaphoreTake(guard,portMAX_DELAY);
            if(history.token!=token) { memset(history.items,0,sizeof(history.items)); history.token=token; }
            history.count=total;
            memcpy(history.items[index],b+7,n-7); history.items[index][n-7]=0; ++history.revision;
            xSemaphoreGive(guard); return;
        }
        // Destination packets are deliberately compact: title/address are one
        // UTF-8 field (bounded by the phone to fit the negotiated MTU).
        if(n>=8 && b[0]==0x50 && word(b+1)!=session.load()) { ++destinationStale; destinationLastPhase=b[5]; return; }
        if(n>=8 && b[0]==0x50 && word(b+1)==session.load()) {
            const uint8_t phase=b[5], index=b[6], total=b[7];
            if(phase>6 || index>=5 || total>5 || memchr(b+8,0,n-8)) return;
            ++destinationAccepted; destinationLastPhase=phase;
            historyWaitingAt=0;
            xSemaphoreTake(guard,portMAX_DELAY);
            destination.session=session.load(); destination.phase=phase; destination.count=total;
            char* target=phase==1 && index<5 ? destination.items[index] : destination.status;
            const size_t capacity=phase==1 && index<5 ? sizeof(destination.items[index]) : sizeof(destination.status);
            const size_t length=std::min(n-8,capacity-1); memcpy(target,b+8,length); target[length]=0;
            ++view.revision;
            xSemaphoreGive(guard);
            return;
        }
        if(n<5 || word(b+1)!=session.load()) return;
        if(n==5 && b[0]==0x10) { phoneReady=session.load(); return; }
        if(n==6 && b[0]==0x11 && b[5]<=4) {
            if(b[5]>=3 || b[5]==0) { held=false; active=false; }
            if(b[5]==0 || b[5]==4) canceled=true;
            xSemaphoreTake(guard,portMAX_DELAY); view.busy=active.load(); ++view.revision; xSemaphoreGive(guard);
            return;
        }
        if(n<9 || b[0]!=0x20 || b[5]>2 || b[6]>1) return;
        const int field=b[5]; const uint16_t sequence=b[7] | uint16_t(b[8])<<8;
        xSemaphoreTake(guard,portMAX_DELAY);
        char* target=field==0 ? view.status : field==1 ? view.question : view.answer;
        const size_t capacity=field==0 ? sizeof(view.status) : field==1 ? sizeof(view.question) : sizeof(view.answer);
        if(b[6]==0 && sequence==0) { target[0]=0; textSequence[field]=0; }
        if(sequence==textSequence[field]) {
            const size_t used=strlen(target);
            if(n-9 < capacity-used && !memchr(b+9,0,n-9)) {
                memcpy(target+used,b+9,n-9); target[used+n-9]=0; ++textSequence[field]; ++view.revision;
            }
        }
        xSemaphoreGive(guard);
    }
} input;
void worker(void*) {
    int16_t stereo[640];
    while(true) {
        if(playing && !held) {
            playbackWorkerBusy=true;
            const uint32_t id=playbackId.load(); const uint16_t peer=playbackPeer.load();
            bool started=false; uint16_t consumed=0; uint32_t startedAt=0;
            playbackWaits=0; playbackLastError=0;
            int16_t output[1280];
            while(playing && playbackId==id && !held) {
                const uint32_t prefill=std::min(uint32_t(10),(playbackExpected.load()+319)/320);
                if(!started && uxQueueMessagesWaiting(playbackQueue)<prefill && !playbackEnded) {
                    if(millis()-playbackAt>5000) { ++playbackErrors; playbackLastError=2; break; }
                    vTaskDelay(pdMS_TO_TICKS(5)); continue;
                }
                PlaybackBlock block{};
                if(xQueueReceive(playbackQueue,&block,pdMS_TO_TICKS(60))!=pdTRUE) {
                    if(playbackEnded && consumed==playbackReceived) break;
                    if(started) ++playbackWaits;
                    if(millis()-playbackAt>1500) { ++playbackErrors; playbackLastError=3; break; }
                    continue;
                }
                if(!started) {
                    i2s_zero_dma_buffer(I2S_NUM_0); i2s_start(I2S_NUM_0); digitalWrite(1,LOW);
                    vTaskDelay(pdMS_TO_TICKS(100)); started=true; startedAt=millis();
                }
                const auto* b=block.bytes; const size_t count=b[10]|uint16_t(b[11])<<8;
                int predictor=int16_t(uint16_t(b[7])|uint16_t(b[8])<<8),index=b[9];
                for(size_t i=0;i<count;++i) {
                    const int16_t sample=i ? hudvoice::decode((b[12+(i-1)/2]>>(((i-1)%2)*4))&15,predictor,index) : predictor;
                    for(int j=0;j<4;++j) output[i*4+j]=sample;
                }
                size_t wrote=0;
                if(i2s_write(I2S_NUM_0,output,count*8,&wrote,pdMS_TO_TICKS(150))!=ESP_OK || wrote!=count*8) { ++playbackErrors; playbackLastError=4; break; }
                ++consumed; ackPlayback(id,peer,1,consumed);
            }
            const bool complete=playing && playbackEnded && consumed==playbackReceived && playbackId==id;
            if(complete) vTaskDelay(pdMS_TO_TICKS(180));
            playbackDuration=started ? millis()-startedAt : 0; playbackExpectedMs=playbackSamples*1000/8000;
            digitalWrite(1,HIGH); if(started) { i2s_stop(I2S_NUM_0); i2s_zero_dma_buffer(I2S_NUM_0); }
            playing=false;
            playbackWorkerBusy=false;
            if(complete) ++playbackCompleted;
            ackPlayback(id,peer,complete ? 2 : 3,consumed);
        }
        if(!held) {
            if(tonePending.exchange(false)) {
                i2s_zero_dma_buffer(I2S_NUM_0); i2s_start(I2S_NUM_0);
                digitalWrite(1,LOW); // SC8002B SHUTDOWN is active HIGH.
                for(int block=0;block<12;++block) {
                    for(int i=0;i<320;++i) stereo[2*i]=stereo[2*i+1]=int16_t(8000*sinf(2*3.14159265f*660*(block*320+i)/16000));
                    size_t wrote=0;
                    if(i2s_write(I2S_NUM_0,stereo,sizeof(stereo),&wrote,pdMS_TO_TICKS(100))!=ESP_OK) break;
                }
                vTaskDelay(pdMS_TO_TICKS(180));
                digitalWrite(1,HIGH); i2s_stop(I2S_NUM_0); i2s_zero_dma_buffer(I2S_NUM_0); toneBusy=false;
            }
            if(active && processingAt && millis()-processingAt>65000) {
                aiVoiceCancel(); state("AI quá thời gian. Hãy thử lại.");
            }
            vTaskDelay(pdMS_TO_TICKS(10)); continue;
        }
        processingAt=0;
        const uint32_t id=session.load();
        uint8_t start[9]={0xA0}; putWord(start+1,id); start[5]=0x80; start[6]=0x3e; start[7]=1; start[8]=voicePurpose.load();
        if(!notify(start,sizeof(start),id)) { held=false; active=false; state("Mở tab AI trong HUD Connect"); continue; }
        const uint32_t waitingAt=millis();
        while(held && !canceled && phoneReady!=id && millis()-waitingAt<20000) vTaskDelay(pdMS_TO_TICKS(10));
        if(!held || canceled || phoneReady!=id) {
            held=false; active=false;
            uint8_t cancel[5]={0xA3}; putWord(cancel+1,id); notify(cancel,sizeof(cancel),id);
            if(!canceled) state("Chưa thu âm. Cho phép nhận dạng trên iPhone.");
            continue;
        }
        i2s_start(I2S_NUM_0);
        i2s_event_t event;
        while(xQueueReceive(i2sEvents,&event,0)==pdTRUE) {}
        // Discard queued samples from before the button was pressed.
        size_t got=0;
        for(int i=0;i<10;++i) { i2s_read(I2S_NUM_0,stereo,sizeof(stereo),&got,0); if(!got) break; }
        recording=true; state("Đang nghe - thả để gửi");
        uint16_t sequence=0;
        int index=0;
        bool failed=false;
        ++recordings; samplesSent=0; peak=0;
        const auto peer=server->getPeerInfo(uint8_t(0));
        const size_t maxSamples=std::min(size_t(320),size_t(std::min(int(peer.getMTU())-3,244)-12)*2+1);
        while(held && !canceled && session==id) {
            got=0;
            const size_t wanted=maxSamples;
            if(i2s_read(I2S_NUM_0,stereo,wanted*4,&got,pdMS_TO_TICKS(100))!=ESP_OK || got<4) { failed=true; break; }
            while(xQueueReceive(i2sEvents,&event,0)==pdTRUE) if(event.type==I2S_EVENT_RX_Q_OVF) failed=true;
            if(failed) break;
            const size_t count=got/4;
            uint8_t packet[244]{}; packet[0]=0xA1; putWord(packet+1,id);
            packet[5]=sequence; packet[6]=sequence>>8;
            int predictor=stereo[0];
            packet[7]=predictor; packet[8]=uint16_t(predictor)>>8; packet[9]=index;
            packet[10]=count; packet[11]=count>>8;
            for(size_t i=0;i<count;++i) {
                const int sample=stereo[i*2]; peak=std::max(peak.load(),abs(sample));
                if(i) packet[12+(i-1)/2] |= hudvoice::encode(sample,predictor,index) << (((i-1)%2)*4);
            }
            if(!notify(packet,12+count/2,id)) { failed=true; break; }
            ++sequence; samplesSent+=count;
        }
        i2s_stop(I2S_NUM_0); recording=false; held=false;
        if(canceled || session!=id) { active=false; continue; }
        uint8_t end[7]={uint8_t(failed ? 0xA3 : 0xA2)}; putWord(end+1,id); end[5]=sequence; end[6]=sequence>>8;
        if(!notify(end,failed ? 5 : 7,id) || failed) {
            active=false; state("Lỗi gửi âm thanh. Thử lại.");
        } else { processingAt=millis(); state("Đang nhận dạng câu nói…"); }
        Serial.printf("AI voice: samples=%u; peak=%d; chunks=%u; failed=%u\n",samplesSent.load(),peak.load(),sequence,failed);
    }
}
}
bool aiVoiceBegin() {
    playbackQueue=xQueueCreate(24,sizeof(PlaybackBlock)); playbackAcks=xQueueCreate(24,sizeof(PlaybackAck));
    toolRequests=xQueueCreate(4,sizeof(AiToolRequest));
    guard=xSemaphoreCreateMutex(); if(!guard) return false;
    session=esp_random() | 1;
    pinMode(1,OUTPUT); digitalWrite(1,HIGH); // Keep SC8002B in shutdown except during playback.
    // Fixed 16 kHz / 16-bit stereo I2S; ES8311 ADC is read from the left slot.
    i2s_config_t config{};
    config.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_RX|I2S_MODE_TX);
    config.sample_rate=16000; config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT; config.communication_format=I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1; config.dma_buf_count=8; config.dma_buf_len=320;
    config.use_apll=false; config.tx_desc_auto_clear=true; config.fixed_mclk=6144000; config.mclk_multiple=I2S_MCLK_MULTIPLE_384;
    i2s_pin_config_t pins{}; pins.mck_io_num=4; pins.bck_io_num=5; pins.ws_io_num=7; pins.data_out_num=8; pins.data_in_num=6;
    bool ok=i2s_driver_install(I2S_NUM_0,&config,8,&i2sEvents)==ESP_OK;
    if(ok) ok=i2s_set_pin(I2S_NUM_0,&pins)==ESP_OK;
    // ES8311 6.144 MHz MCLK / 16 kHz coefficients and ADC configuration,
    // derived from the board vendor's Apache-2.0 Espressif driver.
    if(ok) ok=writeCodec(0x00,0x1f);
    delay(20);
    const uint8_t registers[][2]={{0x00,0x00},{0x00,0x80},{0x01,0x3f},{0x02,0x48},{0x03,0x10},{0x09,0x0c},
        {0x04,0x10},{0x05,0x00},{0x06,0x03},{0x07,0x00},{0x08,0xff},{0x09,0x0c},{0x0a,0x0c},
        {0x0d,0x01},{0x0e,0x02},{0x12,0x00},{0x13,0x10},{0x1c,0x6a},{0x37,0x08},{0x17,0xc8},{0x14,0x1a}};
    for(const auto& reg:registers) if(ok) ok=writeCodec(reg[0],reg[1]);
    i2s_stop(I2S_NUM_0);
    microphone=ok;
    state(ok ? "Giữ nút để hỏi AI" : "Chưa khởi tạo được mic ES8311");
    return xTaskCreate(worker,"ai-microphone",8192,nullptr,2,nullptr)==pdPASS;
}
void aiVoiceAttach(NimBLEServer* instance,NimBLEService* service) {
    server=instance;
    channel=service->createCharacteristic("ee43a105-1b32-4d0f-9ab8-2248b0a01000",
        NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_NR|NIMBLE_PROPERTY::WRITE_ENC|NIMBLE_PROPERTY::NOTIFY);
    channel->setCallbacks(&input);
}
void aiVoiceReset() {
    historyWaitingAt=0;
    playing=false; digitalWrite(1,HIGH);
    ++toolEpoch;
    held=false; canceled=true; active=false; ++session;
    if(guard) {
        xSemaphoreTake(guard,portMAX_DELAY); view.question[0]=view.answer[0]=0; xSemaphoreGive(guard);
        state("Mở HUD Connect, giữ màn hình để nói");
    }
}
bool aiVoiceStart(uint8_t purpose) {
    historyWaitingAt=0;
    playing=false; digitalWrite(1,HIGH);
    if(toneBusy) return false;
    if(!guard || !microphone || !channel || !server || server->getConnectedCount()!=1) return false;
    const auto peer=server->getPeerInfo(uint8_t(0));
    if(!peer.isEncrypted() || peer.getMTU()<100) return false;
    if(active) return false;
    if(purpose > 1) return false;
    ++session; phoneReady=0; canceled=false; processingAt=0; active=true; voicePurpose=purpose;
    xSemaphoreTake(guard,portMAX_DELAY);
    view.question[0]=view.answer[0]=0; memset(textSequence,0,sizeof(textSequence));
    memset(&destination,0,sizeof(destination)); destination.session=session.load();
    xSemaphoreGive(guard);
    state("Đang kết nối nhận dạng giọng nói…"); held=true;
    return true;
}
void aiVoiceStop() { held=false; }
void aiVoiceCancel() {
    held=false; canceled=true; active=false;
    if(channel && server && server->getConnectedCount()==1) {
        const auto peer=server->getPeerInfo(uint8_t(0));
        uint8_t packet[5]={0xA3}; putWord(packet+1,session.load());
        if(peer.isEncrypted()) channel->notify(packet,sizeof(packet),peer.getConnHandle());
    }
    ++session; if(guard) state("Đã hủy câu hỏi");
    if(guard) {
        xSemaphoreTake(guard,portMAX_DELAY); view.question[0]=view.answer[0]=0; ++view.revision; xSemaphoreGive(guard);
    }
}
bool aiVoiceBusy() { return active; }
void aiVoiceRead(AiVoiceView& output) {
    if(!guard) { output={}; return; }
    xSemaphoreTake(guard,portMAX_DELAY); output=view; xSemaphoreGive(guard);
}
void aiVoiceDestinationRead(AiDestinationView& output) {
    if(!guard) { output={}; return; }
    xSemaphoreTake(guard,portMAX_DELAY);
    if(historyWaitingAt && uint32_t(millis()-historyWaitingAt.load())>15000) {
        historyWaitingAt=0; destination.phase=3;
        strlcpy(destination.status,"Chưa nhận phản hồi. Mở HUD Connect rồi thử lại.",sizeof(destination.status)); ++view.revision;
    }
    output=destination; xSemaphoreGive(guard);
}
void aiVoiceReadStatus(char (&status)[128]) {
    status[0]=0;
    if(!guard) return;
    xSemaphoreTake(guard,portMAX_DELAY); strlcpy(status,view.status,sizeof(status)); xSemaphoreGive(guard);
}
void aiVoiceHistoryRead(AiHistoryView& output) {
    if(!guard) { output={}; return; }
    xSemaphoreTake(guard,portMAX_DELAY); output=history; xSemaphoreGive(guard);
    output.connected=server && server->getConnectedCount()==1;
}
bool aiVoiceHistoryControl(uint8_t action,uint8_t index) {
    if(action>1 || !guard || !channel || !server || server->getConnectedCount()!=1 || active) return false;
    const auto peer=server->getPeerInfo(uint8_t(0)); if(!peer.isEncrypted()) return false;
    uint32_t token=0,id=0;
    xSemaphoreTake(guard,portMAX_DELAY);
    if(action && (index>=history.count || !history.items[index][0])) { xSemaphoreGive(guard); return false; }
    token=history.token;
    if(action) {
        id=++session; destination={}; destination.session=id; destination.phase=6; historyWaitingAt=millis();
        strlcpy(destination.status,"Đang chuẩn bị tuyến...",sizeof(destination.status)); ++view.revision;
    }
    xSemaphoreGive(guard);
    uint8_t packet[11]={0x55,action}; putWord(packet+2,id); putWord(packet+6,token); packet[10]=index;
    const bool sent=channel->notify(packet,sizeof(packet),peer.getConnHandle());
    if(!sent && action) {
        historyWaitingAt=0;
        xSemaphoreTake(guard,portMAX_DELAY); destination.phase=3;
        strlcpy(destination.status,"Chưa gửi được lựa chọn. Kiểm tra kết nối rồi thử lại.",sizeof(destination.status)); ++view.revision; xSemaphoreGive(guard);
    }
    return sent;
}
bool aiVoiceDestinationControl(uint8_t action,uint8_t index) {
    uint32_t currentSession=0;
    uint8_t currentCount=0;
    if(!guard) return false;
    xSemaphoreTake(guard,portMAX_DELAY); currentSession=destination.session; currentCount=destination.count; xSemaphoreGive(guard);
    if(action>2 || (action==1 && index>=currentCount) || !server || !channel || server->getConnectedCount()!=1) return false;
    const auto peer=server->getPeerInfo(uint8_t(0));
    if(!peer.isEncrypted() || !currentSession || currentSession!=session.load()) return false;
    uint8_t packet[7]={0x52}; putWord(packet+1,currentSession); packet[5]=action; packet[6]=index;
    const bool sent=channel->notify(packet,sizeof(packet),peer.getConnHandle());
    if(sent && (action==0 || action==2)) {
        historyWaitingAt=0;
        xSemaphoreTake(guard,portMAX_DELAY); destination={}; ++view.revision; xSemaphoreGive(guard);
    }
    return sent;
}
void aiVoicePrintStatus() {
    AiDestinationView current{}; aiVoiceDestinationRead(current);
    Serial.printf("Destination RX: accepted=%u; stale=%u; lastPhase=%u; phase=%u; count=%u; currentSession=%u\n",
        destinationAccepted.load(),destinationStale.load(),unsigned(destinationLastPhase.load()),unsigned(current.phase),unsigned(current.count),current.session==session.load());
    Serial.printf("Speaker: playing=%u; completed=%u; errors=%u; received=%u\n",playing.load(),playbackCompleted.load(),playbackErrors.load(),playbackReceived.load());
    Serial.printf("Speaker timing: ms=%u; expected=%u; waits=%u; error=%u\n",playbackDuration.load(),playbackExpectedMs.load(),playbackWaits.load(),unsigned(playbackLastError.load()));
    Serial.printf("AI voice: codec=%u; busy=%u; recordings=%u; samples=%u; peak=%d; TX_errors=%u\n",
        microphone,active.load(),recordings.load(),samplesSent.load(),peak.load(),txErrors.load());
}
bool aiVoicePollTool(AiToolRequest& request) {
    if(!toolRequests || toolReply.pending || xQueueReceive(toolRequests,&request,0)!=pdTRUE) return false;
    // A repeated packet replays its result instead of executing the action twice.
    if(request.epoch!=toolEpoch.load()) return false;
    if(toolReply.request.id==request.id && toolReply.request.peer==request.peer && toolReply.request.epoch==request.epoch) {
        toolReply.offset=toolReply.sequence=0; toolReply.started=millis(); toolReply.pending=true;
        return false;
    }
    return true;
}
void aiVoiceReplyTool(const AiToolRequest& request,bool success,const char* text) {
    toolReply.request=request; toolReply.success=success;
    strlcpy(toolReply.text,text,sizeof(toolReply.text));
    toolReply.offset=toolReply.sequence=0; toolReply.started=millis(); toolReply.pending=true;
}
void aiVoiceToolTick() {
    static PlaybackAck pending{}; static bool hasAck=false;
    if(!hasAck && playbackAcks) hasAck=xQueueReceive(playbackAcks,&pending,0)==pdTRUE;
    if(hasAck && channel && server) {
        if(!server->getPeerInfoByHandle(pending.peer).isEncrypted()) hasAck=false;
        else {
            uint8_t bytes[8]={0xB1}; putWord(bytes+1,pending.id); bytes[5]=pending.phase;
            bytes[6]=pending.consumed; bytes[7]=pending.consumed>>8;
            if(channel->notify(bytes,sizeof(bytes),pending.peer)) hasAck=false;
        }
    }
    if(!toolReply.pending) return;
    if(!server || !channel || toolReply.request.epoch!=toolEpoch.load() || millis()-toolReply.started>5000) { toolReply.pending=false; return; }
    const auto peer=server->getPeerInfoByHandle(toolReply.request.peer);
    if(!peer.isEncrypted() || !server->getConnectedCount()) { toolReply.pending=false; return; }
    const size_t total=strlen(toolReply.text);
    const size_t count=std::min(total-toolReply.offset,size_t(std::max(20,std::min(244,int(peer.getMTU())-3))-9));
    uint8_t packet[244]{}; packet[0]=0xB0; putWord(packet+1,toolReply.request.id);
    packet[5]=toolReply.success ? 0 : 1;
    packet[6]=toolReply.sequence; packet[7]=toolReply.sequence>>8;
    packet[8]=toolReply.offset+count==total;
    memcpy(packet+9,toolReply.text+toolReply.offset,count);
    if(!channel->notify(packet,9+count,toolReply.request.peer)) return;
    toolReply.offset+=count; ++toolReply.sequence;
    if(toolReply.offset==total) toolReply.pending=false;
}
bool aiVoiceSetSpeakerVolume(uint8_t volume) {
    if(!microphone || volume>100) return false;
    // Map volume to at most 0 dB per ES8311's register table. Only DAC gain
    // changes; microphone ADC gain and its I2S clock stay untouched.
    const uint8_t gain=huddisplay::speakerGain(volume);
    Wire.beginTransmission(0x18); Wire.write(0x31);
    if(Wire.endTransmission(false)!=0 || Wire.requestFrom(uint8_t(0x18),uint8_t(1))!=1) return false;
    const uint8_t current=Wire.read();
    return writeCodec(0x32,gain) && writeCodec(0x31,volume ? current&~0x60 : current|0x60);
}
bool aiVoiceTestSpeaker() {
    if(held || active || toneBusy || playing || !aiVoiceSetSpeakerVolume(hudHardwareSettingsGet().speakerVolume)) return false;
    toneBusy=true; tonePending=true; return true;
}
bool aiVoiceSpeakerBusy() { return playing || toneBusy; }
