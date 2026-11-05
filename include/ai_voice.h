#pragma once
#include <stdint.h>
class NimBLEServer;
class NimBLEService;
struct AiVoiceView {
    uint32_t revision;
    uint32_t session;
    bool busy;
    bool recording;
    char status[128];
    char question[4096];
    char answer[4096];
};
struct AiDestinationView {
    uint32_t session;
    uint8_t phase; // 0 waiting, 1 results, 2 preview, 3 error
    uint8_t count;
    char status[192];
    char items[5][192];
};
struct AiHistoryView {
    uint32_t token=0, revision=0;
    uint8_t count=0;
    bool navigating=false,connected=false;
    char items[5][192]{};
};
void aiVoiceHistoryRead(AiHistoryView& history);
bool aiVoiceHistoryControl(uint8_t action,uint8_t index=0); // 0 refresh, 1 choose
bool aiVoiceBegin(); // Wire already initialized; never installs another I2C driver.
void aiVoiceAttach(NimBLEServer* server, NimBLEService* service);
void aiVoiceReset();
// purpose 0 is an AI question; 1 is an independent Maps destination search.
bool aiVoiceStart(uint8_t purpose = 0);
void aiVoiceStop();
void aiVoiceCancel();
bool aiVoiceBusy();
void aiVoiceRead(AiVoiceView& view);
void aiVoiceDestinationRead(AiDestinationView& view);
void aiVoiceReadStatus(char (&status)[128]);
bool aiVoiceDestinationControl(uint8_t action,uint8_t index=0); // 0 cancel, 1 choose, 2 begin
void aiVoicePrintStatus();
struct AiToolRequest { uint32_t id; uint32_t epoch; uint16_t peer; uint8_t action; uint16_t value; };
bool aiVoicePollTool(AiToolRequest& request);
void aiVoiceReplyTool(const AiToolRequest& request,bool success,const char* text);
void aiVoiceToolTick();
bool aiVoiceSetSpeakerVolume(uint8_t volume);
bool aiVoiceTestSpeaker();
bool aiVoiceSpeakerBusy();
