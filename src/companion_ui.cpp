#include "companion_ui.h"
#include "nav_stream.h"
#include "hud_settings.h"
#include "ai_voice.h"
#include <lunar_calendar.h>

#include <Arduino.h>
#include <companion_fonts.h>
#include <lvgl.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <ai_gesture.h>
#include <hud_destination_ui.h>
#include <ai_typewriter.h>

namespace {
uint32_t BG = 0x080E18;
uint32_t PANEL = 0x111E30;
uint32_t CYAN = 0x41DAF5;
uint32_t WHITE = 0xEBF3FC;
uint32_t MUTED = 0x8C9FB5;
uint32_t BUTTON = 0x214358;
int viewWidth=320,viewHeight=240,pageWidth=300,pageHeight=200;
lv_obj_t *inboxDeleteAll,*aiCloseButton,*aiHint,*aiListeningHint,*popupCloseButton,*mapPortraitHint;
uint32_t OVERLAY = 0x173347;
struct Palette { uint32_t bg, panel, accent, text, muted, button, overlay; };
lv_obj_t* root;
constexpr int PAGE_COUNT = 5;
lv_obj_t* pages[PAGE_COUNT];
lv_obj_t* pageDots[PAGE_COUNT];
lv_obj_t* pageHeading;
lv_obj_t* smallClock;
lv_obj_t* lunarLabel;
uint32_t swipeCount = 0;
lv_obj_t* statusDot;
lv_obj_t* clockLabel;
lv_obj_t* dateLabel;
lv_obj_t* extraLabel;
lv_obj_t* analog;
lv_obj_t* hands[3];
lv_point_precise_t handPoints[3][2];
constexpr uint8_t SPEED_DIGITAL_FACE = 6, SPEED_DIAL_FACE = 7;
lv_obj_t *speedDigital, *speedDigitalValue, *speedDigitalStatus, *speedDigitalClock, *speedDigitalDate;
lv_obj_t *speedDial, *speedDialValue, *speedDialClock, *speedDialNeedle;
lv_point_precise_t speedDialNeedlePoints[2];
float speedDialDisplayed = -1;
uint32_t speedDialUpdatedAt = 0;
uint32_t appliedRevision = 0;
lv_obj_t* noticePopup;
lv_obj_t* popupApp;
lv_obj_t* popupText;
uint32_t popupUntil = 0;
uint32_t popupUid = 0;
lv_obj_t* inboxApp;
lv_obj_t* inboxTitle;
lv_obj_t* inboxBody;
lv_obj_t* inboxCount;
lv_obj_t* refreshButton;
lv_obj_t* refreshCaption;
lv_obj_t* inboxList;
lv_obj_t* inboxDetail;
lv_obj_t* inboxTime;
lv_obj_t* detailDelete;
lv_obj_t* inboxRows[10];
lv_obj_t* rowApps[10];
lv_obj_t* rowTimes[10];
lv_obj_t* rowTitles[10];
lv_obj_t* rowContents[10];
lv_obj_t* deleteDialog;
lv_obj_t* deletePrompt;
bool detailOpen = false;
uint32_t detailUid = 0;
bool deletingAll = false;
uint32_t deleteUid = 0;
uint32_t hiddenUids[64];
size_t hiddenCount = 0;
uint64_t hiddenFingerprints[64];
size_t hiddenFingerprintCount = 0;
lv_obj_t* speedBox;
lv_obj_t* speedLabel;
lv_obj_t* mapTitle;
lv_obj_t* mapBody;
lv_obj_t* mapSource;
lv_obj_t* mapImage;
lv_obj_t* mapControls;
lv_obj_t* mapPanButton;
lv_obj_t *mapMenuButton,*mapStopDialog,*mapStopMessage;
bool mapMenuOpen=false,mapStopPending=false;
uint32_t mapMenuAt=0,mapStopAt=0;
lv_obj_t *mapSearchButton,*mapSearchPanel,*mapSearchStatus,*mapSearchMic,*mapSearchStart,*mapSearchCancel;
HudDestinationUi mapSearchUi;
HudMapHomeUi mapHomeUi;
AiHistoryView mapHistoryState;
bool mapTripActive=false,mapHomeRecording=false;
uint32_t mapHistoryAppliedRevision=UINT32_MAX;
AiDestinationView mapSearchState;
uint32_t mapSearchRevision=UINT32_MAX;
uint32_t mapSearchCompletedSession=0;
uint32_t mapSearchPreviewSession=0;
uint32_t mapSearchResultsSession=0;
lv_obj_t* aiStatus;
lv_obj_t* aiChatList;
lv_obj_t* aiEmpty;
lv_obj_t* aiListening;
lv_obj_t* aiPulse;
lv_obj_t* aiListenText;
lv_obj_t* aiListenCircle;
constexpr int AI_PAGE=4, NORMAL_PAGES=4, AI_TURNS=4;
struct AiTurn { uint32_t session; uint16_t shownAnswerBytes; char question[4096]; char answer[4096]; };
uint32_t aiTypingAt=0;
bool aiTypingStatus=false;
AiTurn* aiTurns=nullptr;
size_t aiTurnCount=0;
lv_obj_t* aiRows[AI_TURNS*2];
lv_obj_t* aiBubbles[AI_TURNS*2];
lv_obj_t* aiBodies[AI_TURNS*2];
hudvoice::AiGesture aiGesture;
hudvoice::Gesture pendingAiGesture=hudvoice::Gesture::None;
bool aiHolding=false;
int aiReturnPage=0;
uint32_t aiRevision=UINT32_MAX;
bool mapPanMode = false;
bool mapDragging = false;
lv_point_t mapDragStart{};
lv_obj_t* googleOverlay;
lv_obj_t* googleHeading;
lv_obj_t* googleText;
phone::Notice googleGuide;
bool hasGoogleGuide = false;
uint32_t googleGuideAt = 0;
lv_image_dsc_t mapFrame{};
uint32_t mapReceivedAt = 0;
bool mapLive = false;
lv_obj_t* pairingPanel;
lv_obj_t* pairingCode;
lv_obj_t* musicTitle;
lv_obj_t* musicArtist;
lv_obj_t* musicStatus;
lv_obj_t* musicVolume;
constexpr int MUSIC_BUTTON_COUNT = 5;
lv_obj_t* musicButtons[MUSIC_BUTTON_COUNT];
lv_obj_t* musicPlayIcon;
bool musicAvailable = false;
bool musicPlaying = false;
bool musicPlaybackKnown=false;
int musicVolumePercent = -1;
uint16_t supportedMusicCommands = 0;
uint32_t musicFeedbackUntil = 0;
struct InboxEntry : phone::Notice {
    int64_t receivedUnix = 0;
    uint32_t receivedMillis = 0;
    bool deleting = false;
    uint32_t deleteAt = 0;
};
InboxEntry notices[11]; // One temporary candidate before retaining the newest ten.
size_t count = 0;
size_t selected = 0;
int currentPage = 0;
bool clockValid = false;
int32_t utcOffset = 7 * 3600;
int64_t renderedSecond = -1;
uint32_t lastNavigationUid = 0;
bool hasNavigationNotice = false;
phone::Notice navigationNotice;
bool refreshBusy = false;
uint32_t feedbackUntil = 0;
const lv_font_t* textFont = &hud_font_vietnamese;
bool homeFontsReady = false;
void clearMaps(bool force);
void updateGoogleOverlay();
void refreshInbox();

uint64_t noticeFingerprint(const phone::Notice& notice) {
    // Source date distinguishes a fresh identical message from an old row
    // replayed under a new ANCS UID. Without a date, don't guess across sessions.
    if(!phone::notificationDateKey(notice.date)) return 0;
    uint64_t hash=14695981039346656037ULL;
    const char* fields[]={notice.app,notice.date,notice.title,notice.message};
    for(const char* field:fields) {
        for(const unsigned char* p=reinterpret_cast<const unsigned char*>(field);*p;++p) { hash^=*p; hash*=1099511628211ULL; }
        hash^=0; hash*=1099511628211ULL;
    }
    return hash;
}

void recolor(lv_obj_t* obj, const Palette& old) {
    const uint32_t bg = lv_color_to_u32(lv_obj_get_style_bg_color(obj, LV_PART_MAIN)) & 0xffffff;
    if (bg == old.bg) lv_obj_set_style_bg_color(obj, lv_color_hex(BG), 0);
    else if (bg == old.panel) lv_obj_set_style_bg_color(obj, lv_color_hex(PANEL), 0);
    else if (bg == old.button) lv_obj_set_style_bg_color(obj, lv_color_hex(BUTTON), 0);
    else if (bg == old.overlay) lv_obj_set_style_bg_color(obj, lv_color_hex(OVERLAY), 0);
    else if (bg == old.muted) lv_obj_set_style_bg_color(obj, lv_color_hex(MUTED), 0);
    else if (bg == old.accent) lv_obj_set_style_bg_color(obj, lv_color_hex(CYAN), 0);
    const uint32_t color = lv_color_to_u32(lv_obj_get_style_text_color(obj, LV_PART_MAIN)) & 0xffffff;
    if (color == old.accent) lv_obj_set_style_text_color(obj, lv_color_hex(CYAN), 0);
    else if (color == old.text) lv_obj_set_style_text_color(obj, lv_color_hex(WHITE), 0);
    else if (color == old.muted) lv_obj_set_style_text_color(obj, lv_color_hex(MUTED), 0);
    else if (color == old.bg) lv_obj_set_style_text_color(obj, lv_color_hex(BG), 0);
    if((lv_color_to_u32(lv_obj_get_style_border_color(obj,LV_PART_MAIN)) & 0xffffff)==old.accent)
        lv_obj_set_style_border_color(obj,lv_color_hex(CYAN),0);
    if((lv_color_to_u32(lv_obj_get_style_line_color(obj,LV_PART_MAIN)) & 0xffffff)==old.accent)
        lv_obj_set_style_line_color(obj,lv_color_hex(CYAN),0);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        recolor(lv_obj_get_child(obj, i), old);
}

void applySettings() {
    const auto& config = hudSettingsGet();
    const Palette palettes[] = {
        {0x080E18,0x111E30,0x41DAF5,0xEBF3FC,0x8C9FB5,0x214358,0x173347},
        {0x071510,0x10291F,0x63EBAB,0xEBF3FC,0x8C9FB5,0x214358,0x173347},
        {0x181008,0x302116,0xFFBC66,0xEBF3FC,0x8C9FB5,0x214358,0x173347},
        {0x150C19,0x2B1830,0xF4A0D5,0xEBF3FC,0x8C9FB5,0x214358,0x173347},
        {0xEDF3FA,0xFFFFFF,0x0069A8,0x142033,0x52657A,0xDFEAF5,0xE5EFF9},
        {0xECF6F0,0xFFFFFF,0x007A50,0x142B21,0x526B5D,0xDDEFE5,0xE5F3EB},
        {0xFFF5E8,0xFFFCF5,0x995000,0x352515,0x786448,0xF4E5CF,0xFFF0D8},
        {0xFFF0F5,0xFFFFFF,0xA32665,0x35202D,0x755B6C,0xF4DEEA,0xFBE8F1}
    };
    const Palette old{BG,PANEL,CYAN,WHITE,MUTED,BUTTON,OVERLAY};
    const auto& next = palettes[config.theme];
    BG=next.bg; PANEL=next.panel; CYAN=next.accent; WHITE=next.text;
    MUTED=next.muted; BUTTON=next.button; OVERLAY=next.overlay;
    recolor(root, old); recolor(noticePopup, old); recolor(pairingPanel, old);
    lv_obj_set_style_border_color(noticePopup, lv_color_hex(CYAN), 0);
    lv_obj_set_style_border_color(pairingPanel, lv_color_hex(CYAN), 0);
    if(mapPanButton) lv_obj_set_style_border_color(mapPanButton,lv_color_hex(CYAN),LV_STATE_CHECKED);
    if(aiPulse) {
        lv_obj_set_style_bg_color(aiPulse,lv_color_hex(CYAN),0);
        lv_obj_set_style_border_color(aiPulse,lv_color_hex(CYAN),0);
        lv_obj_set_style_bg_color(aiListenCircle,lv_color_hex(CYAN),0);
        lv_obj_set_style_text_color(aiListenText,lv_color_hex(BG),0);
    }
    for (int i = 0; i < PAGE_COUNT; ++i) {
        lv_obj_set_style_bg_color(pageDots[i], lv_color_hex(i == currentPage ? CYAN : MUTED), 0);
    }
    const bool speedDigitalFace = config.face == SPEED_DIGITAL_FACE;
    const bool speedDialFace = config.face == SPEED_DIAL_FACE;
    const bool speedFace = speedDigitalFace || speedDialFace;
    const bool round = config.face == 2 || config.face == 5;
    const bool classic = config.face == 5;
    lv_obj_set_hidden(analog, !round || speedFace);
    lv_obj_set_x(analog, classic ? 78 : 8);
    lv_obj_set_y(analog, classic ? 4 : 20);
    lv_obj_set_hidden(clockLabel, classic || speedFace);
    lv_obj_set_pos(clockLabel, round ? 174 : 0, round ? 46 : (config.face >= 3 ? 4 : 32));
    lv_obj_set_width(clockLabel, round ? 118 : 300);
    lv_obj_set_style_text_font(clockLabel, round || config.face == 4 ? &lv_font_montserrat_34 : &hud_font_clock_80, 0);
    lv_obj_set_style_text_color(clockLabel, lv_color_hex(config.face == 1 ? CYAN : WHITE), 0);
    lv_obj_set_pos(dateLabel, round && !classic ? 174 : 0, classic ? 150 : round ? 103 : config.face >= 3 ? 150 : 140);
    lv_obj_set_width(dateLabel, round && !classic ? 118 : 300);
    lv_obj_set_style_text_font(dateLabel, round || config.face >= 3 ? textFont : &hud_font_vietnamese_24, 0);
    lv_obj_set_style_text_color(dateLabel, lv_color_hex(config.face == 1 ? WHITE : CYAN), 0);
    for (int i = 0; i < 3; ++i) lv_obj_set_style_line_color(hands[i], lv_color_hex(i == 2 ? CYAN : WHITE), 0);
    lv_obj_set_hidden(extraLabel, (config.face != 3 && config.face != 4) || speedFace);
    lv_obj_set_y(extraLabel, config.face == 4 ? 48 : 104);
    lv_obj_set_style_text_font(extraLabel, config.face == 4 ? &hud_font_clock_80 : &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(extraLabel, lv_color_hex(CYAN), 0);
    lv_obj_set_hidden(dateLabel, speedFace);
    lv_obj_set_hidden(lunarLabel, speedFace);
    lv_obj_set_hidden(speedDigital, !speedDigitalFace);
    lv_obj_set_hidden(speedDial, !speedDialFace);
    lv_obj_set_style_text_color(speedDigitalValue, lv_color_hex(WHITE), 0);
    lv_obj_set_style_text_color(speedDigitalClock, lv_color_hex(CYAN), 0);
    lv_obj_set_style_text_color(speedDigitalDate, lv_color_hex(MUTED), 0);
    lv_obj_set_style_text_color(speedDialValue, lv_color_hex(WHITE), 0);
    lv_obj_set_style_text_color(speedDialClock, lv_color_hex(CYAN), 0);
    lv_obj_set_style_line_color(speedDialNeedle, lv_color_hex(CYAN), 0);
    if(viewHeight>viewWidth) {
        lv_point_t size{}; lv_text_get_size(&size,"88:88",&hud_font_clock_80,0,0,1000,LV_TEXT_FLAG_NONE);
        const lv_font_t* digits=size.x<=pageWidth ? &hud_font_clock_80 : &lv_font_montserrat_48;
        lv_obj_set_pos(analog,(pageWidth-144)/2,10);
        lv_obj_set_pos(clockLabel,0,round ? 163 : config.face==4 ? 4 : 40);
        lv_obj_set_width(clockLabel,pageWidth);
        lv_obj_set_style_text_font(clockLabel,round || config.face==4 ? &lv_font_montserrat_34 : digits,0);
        lv_obj_set_pos(dateLabel,0,round ? (classic ? 177 : 207) : 187);
        lv_obj_set_width(dateLabel,pageWidth);
        lv_obj_set_style_text_font(dateLabel,round ? textFont : &hud_font_vietnamese_24,0);
        lv_obj_set_y(extraLabel,config.face==4 ? 65 : 133);
    }
    // Existing inbox rows must obey a newly changed filter immediately.
    for (size_t i = 0; i < count;) {
        if (hudSettingsAllowNotification(notices[i].app)) { ++i; continue; }
        for (size_t j = i + 1; j < count; ++j) notices[j-1] = notices[j];
        --count;
    }
    appliedRevision = hudSettingsRevision();
    if (hasNavigationNotice && !mapLive && !hudSettingsAllowNotification(navigationNotice.app)) clearMaps(true);
    renderedSecond = -1;
}

bool fontRendersGlyph(uint32_t codepoint, const lv_font_t* font = &hud_font_vietnamese) {
    lv_font_glyph_dsc_t glyph{};
    if (!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0) ||
        !glyph.box_w || !glyph.box_h) return false;
    auto buffer = lv_draw_buf_create(glyph.box_w, glyph.box_h, LV_COLOR_FORMAT_A8, 0);
    if (!buffer) return false;
    memset(buffer->data, 0, buffer->data_size);
    const bool decoded = lv_font_get_glyph_bitmap(&glyph, buffer) != nullptr;
    bool visible = false;
    if (decoded) {
        for (uint32_t i = 0; i < buffer->data_size; ++i) {
            if (buffer->data[i]) { visible = true; break; }
        }
    }
    lv_font_glyph_release_draw_data(&glyph);
    lv_draw_buf_destroy(buffer);
    return decoded && visible;
}

lv_obj_t* label(lv_obj_t* parent, int x, int y, int width, const char* text,
                uint32_t color = WHITE, const lv_font_t* font = nullptr) {
    auto obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_obj_set_style_text_font(obj, font ? font : textFont, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text);
    return obj;
}

lv_obj_t* box(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color) {
    auto obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_event_bubble(obj, true);
    lv_obj_set_gesture_bubble(obj, true);
    return obj;
}

void showPage(int index) {
    currentPage = (index + PAGE_COUNT) % PAGE_COUNT;
    if(currentPage==2 && !mapTripActive && !mapHomeRecording) aiVoiceHistoryControl(0);
    if (noticePopup) {
        lv_obj_set_hidden(noticePopup, true);
        popupUntil = 0;
    }
    for (int i = 0; i < PAGE_COUNT; ++i) {
        lv_obj_set_hidden(pages[i], i != currentPage);
        lv_obj_set_hidden(pageDots[i],viewHeight>viewWidth || i==AI_PAGE || currentPage==AI_PAGE);
        lv_obj_set_style_bg_color(pageDots[i], lv_color_hex(i == currentPage ? CYAN : MUTED), 0);
    }
    const char* names[] = {"ĐỒNG HỒ", "THÔNG BÁO", "BẢN ĐỒ", "NHẠC", "TRỢ LÝ AI"};
    lv_label_set_text(pageHeading, currentPage==2 && mapPanMode ? "KÉO BẢN ĐỒ" : names[currentPage]);
    if (deleteDialog && currentPage != 1) lv_obj_set_hidden(deleteDialog, true);
}

bool isNavigationApp(const char* id) {
    return phone::isGoogleMaps(id) || strcmp(id, "com.apple.Maps") == 0;
}

const char* appName(const char* id) {
    const char* resolved=hudSettingsAppName(id);
    if(resolved[0]) return resolved;
    if (phone::isGoogleMaps(id)) return "Google Maps";
    if (strcmp(id, "com.apple.Maps") == 0) return "Apple Maps";
    if (strcmp(id, "com.apple.MobileSMS") == 0) return "Tin nhắn";
    if (strcmp(id, "com.apple.mobilephone") == 0) return "Điện thoại";
    if (strstr(id, "zalo") || strstr(id, "Zalo")) return "Zalo";
    if (strstr(id, "Messenger")) return "Messenger";
    if (strstr(id, "WhatsApp")) return "WhatsApp";
    return id[0] ? id : "Thông báo iPhone";
}

void updateInboxCount() {
    if (refreshBusy) {
        lv_label_set_text(inboxCount, "Đang làm mới...");
    } else if (!feedbackUntil) {
        if (count) lv_label_set_text_fmt(inboxCount, "%u thông báo", unsigned(count));
        else lv_label_set_text(inboxCount, "0 thông báo");
    }
}

void receivedTime(const InboxEntry& entry, char* text, size_t capacity, bool withDate) {
    int64_t seconds = entry.receivedUnix;
    if (!seconds && clockValid) seconds = time(nullptr) - uint32_t(millis()-entry.receivedMillis)/1000;
    if (!seconds) { snprintf(text,capacity,"--:--"); return; }
    const time_t local=seconds+utcOffset; tm date{}; gmtime_r(&local,&date);
    if (withDate) snprintf(text,capacity,"%02d/%02d %02d:%02d",date.tm_mday,date.tm_mon+1,date.tm_hour,date.tm_min);
    else snprintf(text,capacity,"%02d:%02d",date.tm_hour,date.tm_min);
}

bool sourceTime(const InboxEntry& entry, char* text, size_t capacity, bool withDate) {
    const char* value=entry.date;
    if (!phone::notificationDateKey(value)) return false;
    if (withDate) snprintf(text,capacity,"%.2s/%.2s %.2s:%.2s",value+6,value+4,value+9,value+11);
    else snprintf(text,capacity,"%.2s:%.2s",value+9,value+11);
    return true;
}

void sortInbox() {
    auto key=[](const InboxEntry& entry,char* text) {
        char checked[32];
        if(sourceTime(entry,checked,sizeof(checked),true)) { memcpy(text,entry.date,16); return; }
        if(entry.receivedUnix) {
            const time_t local=entry.receivedUnix+utcOffset; tm date{}; gmtime_r(&local,&date);
            strftime(text,16,"%Y%m%dT%H%M%S",&date);
        } else text[0]=0;
    };
    std::stable_sort(notices,notices+count,[&](const InboxEntry& a,const InboxEntry& b) {
        char first[16]{},second[16]{}; key(a,first); key(b,second);
        if(first[0] && second[0]) { const int compare=strcmp(first,second); if(compare) return compare>0; }
        else if(bool(first[0])!=bool(second[0])) return first[0]!=0;
        return int32_t(a.receivedMillis-b.receivedMillis)>0;
    });
    if(count>10) count=10;
}

void setInboxPreview(lv_obj_t* target, const char* text) {
    // Notification fields are bounded to 511 UTF-8 bytes. Flatten source line
    // breaks so the list always has one title line and one content line.
    char line[512];
    size_t used=0;
    for(const char* at=text; *at && used+1<sizeof(line); ++at) {
        const char value=(*at=='\r' || *at=='\n' || *at=='\t') ? ' ' : *at;
        if(value==' ' && (!used || line[used-1]==' ')) continue;
        line[used++]=value;
    }
    while(used && line[used-1]==' ') --used;
    line[used]=0;
    lv_label_set_text(target,line);
}

void refreshInbox() {
    if (!inboxList) return;
    sortInbox();
    bool found=false;
    for (size_t i=0;i<count;++i) if (notices[i].uid==detailUid) { selected=i; found=true; }
    if (detailOpen && !found) detailOpen=false;
    lv_obj_set_hidden(inboxDetail,!detailOpen);
    lv_obj_set_hidden(inboxList,detailOpen);
    for (size_t i=0;i<10;++i) {
        lv_obj_set_hidden(inboxRows[i], i>=count);
        if (i>=count) continue;
        const auto& entry=notices[i];
        lv_label_set_text(rowApps[i],appName(entry.app));
        char stamp[32]; if (!sourceTime(entry,stamp,sizeof(stamp),false)) receivedTime(entry,stamp,sizeof(stamp),false);
        lv_label_set_text(rowTimes[i],stamp);
        setInboxPreview(rowTitles[i],entry.deleting ? "Đang xóa trên iPhone..." : entry.title[0] ? entry.title : "Thông báo");
        setInboxPreview(rowContents[i],entry.message[0] ? entry.message : "Không có nội dung xem trước");
    }
    if (detailOpen) {
        const auto& entry=notices[selected];
        lv_label_set_text(inboxApp,appName(entry.app));
        lv_label_set_text(inboxTitle,entry.title);
        lv_label_set_text(inboxBody,entry.message[0] ? entry.message : "iPhone không chia sẻ nội dung xem trước.");
        char arrived[32], source[32]; receivedTime(entry,arrived,sizeof(arrived),true);
        if (sourceTime(entry,source,sizeof(source),true)) lv_label_set_text_fmt(inboxTime,"iPhone: %s\nNhận: %s",source,arrived);
        else lv_label_set_text_fmt(inboxTime,"Nhận: %s",arrived);
        if (entry.deleting) lv_obj_add_state(detailDelete,LV_STATE_DISABLED);
        else lv_obj_remove_state(detailDelete,LV_STATE_DISABLED);
    }
    updateInboxCount();
}

void clearMaps(bool force = false) {
    if (mapLive && !force) return;
    mapLive = false;
    mapMenuOpen=false; mapStopPending=false;
    if(mapStopDialog) lv_obj_set_hidden(mapStopDialog,true);
    mapReceivedAt = 0;
    if (mapImage) lv_obj_set_hidden(mapImage, true);
    if (mapControls) lv_obj_set_hidden(mapControls, true);
    if(mapPortraitHint) lv_obj_set_hidden(mapPortraitHint,true);
    if(mapTitle) lv_obj_set_hidden(lv_obj_get_parent(mapTitle),false);
    if(mapSource) lv_obj_set_hidden(mapSource,false);
    mapPanMode = mapDragging = false;
    if (mapPanButton) lv_obj_remove_state(mapPanButton, LV_STATE_CHECKED);
    if (mapImage) lv_obj_set_gesture_bubble(mapImage, true);
    if (currentPage==2 && pageHeading) lv_label_set_text(pageHeading,"BẢN ĐỒ");
    if (googleOverlay) lv_obj_set_hidden(googleOverlay, true);
    hasNavigationNotice = false;
    navigationNotice = {};
    lv_label_set_text(mapSource, "MAPS");
    lv_label_set_text(mapTitle, "Chờ bản đồ");
    lv_label_set_text(mapBody, "Chạm kính lúp để tìm điểm đến trên HUD.");
}

void updateGoogleOverlay() {
    if (!googleOverlay) return;
    static uint32_t lastSecond = UINT32_MAX;
    const uint32_t second = millis() / 1000;
    if (second == lastSecond) return;
    lastSecond = second;
    const bool visible = mapLive && navStreamExternalGuidance();
    lv_obj_set_hidden(googleOverlay, !visible);
    if (!visible) return;
    const bool allowed = hasGoogleGuide && hudSettingsAllowNotification(googleGuide.app);
    if (!allowed) {
        lv_label_set_text(googleHeading, "Google Maps");
        lv_label_set_text(googleText, "Chờ thông báo chỉ đường...");
    } else if (millis() - googleGuideAt > 120000) {
        lv_label_set_text(googleHeading, "Google Maps");
        lv_label_set_text(googleText, "Chỉ dẫn đã cũ. Chờ cập nhật...");
    } else {
        lv_label_set_text(googleHeading, googleGuide.title[0] ? googleGuide.title : "Google Maps");
        lv_label_set_text(googleText, googleGuide.message[0] ? googleGuide.message : "Không có nội dung xem trước.");
    }
}

void showNavigationNotice(const phone::Notice& notice) {
    // Keep the original text: ANCS is not a route feed, so do not invent a
    // maneuver, distance or map when Google supplies an unrelated notification.
    navigationNotice = notice;
    hasNavigationNotice = true;
    lastNavigationUid = notice.uid;
    mapLive = false;
    if (mapImage) lv_obj_set_hidden(mapImage, true);
    lv_label_set_text(mapSource, appName(notice.app));
    lv_label_set_text(mapTitle, notice.title[0] ? notice.title : appName(notice.app));
    lv_label_set_text(mapBody, notice.message[0] ? notice.message :
                      "iPhone chưa chia sẻ nội dung xem trước.");
    lv_obj_scroll_to_y(lv_obj_get_parent(mapTitle), 0, LV_ANIM_OFF);
}

void status(uint32_t color) {
    lv_obj_set_style_bg_color(statusDot, lv_color_hex(color), 0);
}

void setRefreshBusy(bool busy) {
    refreshBusy = busy;
    if (busy) lv_obj_add_state(refreshButton, LV_STATE_DISABLED);
    else lv_obj_remove_state(refreshButton, LV_STATE_DISABLED);
    lv_label_set_text(refreshCaption, LV_SYMBOL_REFRESH);
    updateInboxCount();
}

void feedback(const char* text) {
    feedbackUntil = millis() + 3000;
    lv_label_set_text(inboxCount, text);
}

void onRefresh(lv_event_t*) {
    switch (phoneLinkRefreshNotifications()) {
    case PhoneRefreshResult::Queued:
    case PhoneRefreshResult::Busy:
        feedbackUntil = 0;
        setRefreshBusy(true);
        break;
    case PhoneRefreshResult::Disconnected:
        feedback("Chưa kết nối");
        break;
    case PhoneRefreshResult::Pairing:
        feedback("Chờ ghép đôi");
        break;
    }
}

void openNotice(lv_event_t* event) {
    const size_t index=reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index>=count) return;
    detailUid=notices[index].uid; detailOpen=true;
    lv_obj_scroll_to_y(lv_obj_get_parent(inboxTitle),0,LV_ANIM_OFF);
    refreshInbox();
}

void backToList(lv_event_t*) { detailOpen=false; refreshInbox(); }

void removeLocal(size_t index) {
    const uint32_t uid=notices[index].uid;
    const auto fingerprint=noticeFingerprint(notices[index]);
    bool remembered=false;
    for(size_t i=0;i<hiddenFingerprintCount;++i) if(hiddenFingerprints[i]==fingerprint) remembered=true;
    if(fingerprint && !remembered) {
        if(hiddenFingerprintCount==64) { memmove(hiddenFingerprints,hiddenFingerprints+1,63*sizeof(uint64_t)); --hiddenFingerprintCount; }
        hiddenFingerprints[hiddenFingerprintCount++]=fingerprint;
    }
    if (hiddenCount==64) { memmove(hiddenUids,hiddenUids+1,63*sizeof(uint32_t)); --hiddenCount; }
    hiddenUids[hiddenCount++]=uid;
    if (hasGoogleGuide && googleGuide.uid==uid) hasGoogleGuide=false;
    if (popupUntil && popupUid==uid) { popupUntil=0; lv_obj_set_hidden(noticePopup,true); }
    for (size_t i=index+1;i<count;++i) notices[i-1]=notices[i];
    --count;
    if (hasNavigationNotice && lastNavigationUid==uid && !mapLive) clearMaps(false);
}

void requestDelete(size_t index) {
    auto& entry=notices[index];
    if (entry.deleting) return;
    if (entry.canDismiss) {
        if (phoneLinkDismiss(entry)) { entry.deleting=true; entry.deleteAt=millis(); }
        else feedback("Chưa gửi được lệnh xóa");
    } else { removeLocal(index); feedback("Đã ẩn trên HUD"); }
}

void confirmDelete(lv_event_t*) {
    lv_obj_set_hidden(deleteDialog,true);
    if (deletingAll) { for(size_t i=count;i>0;--i) requestDelete(i-1); }
    else for(size_t i=0;i<count;++i) if(notices[i].uid==deleteUid) { requestDelete(i); break; }
    refreshInbox();
}
void cancelDelete(lv_event_t*) { lv_obj_set_hidden(deleteDialog,true); }
void askDelete(lv_event_t* event) {
    deletingAll=lv_event_get_user_data(event)!=nullptr;
    if (!count) return;
    deleteUid=detailUid;
    bool supported=false;
    for(size_t i=0;i<count;++i) if(notices[i].uid==deleteUid) supported=notices[i].canDismiss;
    lv_label_set_text(deletePrompt,deletingAll ? "Xóa tất cả? Mục iPhone không hỗ trợ sẽ chỉ ẩn trên HUD." :
        supported ? "Xóa thông báo này trên iPhone?" : "iPhone không hỗ trợ xóa mục này. Chỉ ẩn trên HUD?");
    lv_obj_set_hidden(deleteDialog,false);
}

void hidePopup() {
    popupUntil = 0;
    lv_obj_set_hidden(noticePopup, true);
}

void onPopup(lv_event_t*) {
    detailUid=popupUid; detailOpen=true;
    refreshInbox();
    showPage(1);
}

void onPopupClose(lv_event_t*) { hidePopup(); }

void showNoticePopup(const phone::Notice& notice) {
    if (notice.preExisting || currentPage == 1 || lv_obj_is_visible(pairingPanel)) return;
    if (currentPage == 2 && isNavigationApp(notice.app)) return;
    popupUid = notice.uid;
    popupUntil = millis() + 8000;
    lv_label_set_text(popupApp, appName(notice.app));
    lv_label_set_text_fmt(popupText, "%s\n%s", notice.title[0] ? notice.title : "Thông báo mới",
                         notice.message[0] ? notice.message : "Không có nội dung xem trước");
    lv_obj_set_hidden(noticePopup, false);
}

PhoneMusicCommand playCommand() {
    if (supportedMusicCommands & (1 << unsigned(PhoneMusicCommand::Toggle))) return PhoneMusicCommand::Toggle;
    return musicPlaying ? PhoneMusicCommand::Pause : PhoneMusicCommand::Play;
}

void updateMusicControls() {
    const PhoneMusicCommand commands[] = {PhoneMusicCommand::VolumeDown, PhoneMusicCommand::Previous,
        playCommand(), PhoneMusicCommand::Next, PhoneMusicCommand::VolumeUp};
    for (int i = 0; i < MUSIC_BUTTON_COUNT; ++i) {
        if (musicAvailable && (supportedMusicCommands & (1 << unsigned(commands[i]))))
            lv_obj_remove_state(musicButtons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(musicButtons[i], LV_STATE_DISABLED);
    }
    lv_label_set_text(musicPlayIcon, musicPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

void musicHint(const char* text) {
    musicFeedbackUntil = millis() + 2500;
    lv_label_set_text(musicStatus, text);
}

void onMusicCommand(lv_event_t* event) {
    const intptr_t button = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
    const PhoneMusicCommand commands[] = {PhoneMusicCommand::VolumeDown, PhoneMusicCommand::Previous,
        playCommand(), PhoneMusicCommand::Next, PhoneMusicCommand::VolumeUp};
    if (button < 0 || button >= MUSIC_BUTTON_COUNT) return;
    const auto command = commands[button];
    musicHint(phoneLinkMusicCommand(command) ? "Đang gửi..." : "Chưa điều khiển được");
}

void resetMusic() {
    musicPlaybackKnown=false;
    musicAvailable = musicPlaying = false;
    supportedMusicCommands = 0;
    musicVolumePercent = -1;
    musicFeedbackUntil = 0;
    lv_label_set_text(musicTitle, "Mở nhạc trên iPhone");
    lv_label_set_text(musicArtist, "Tên bài và ca sĩ sẽ hiện ở đây");
    lv_label_set_text(musicStatus, "Chờ trình phát nhạc");
    lv_label_set_text(musicVolume, LV_SYMBOL_VOLUME_MID " --%");
    updateMusicControls();
}

void onGesture(lv_event_t* event) {
    if(currentPage==AI_PAGE || aiGesture.consumes()) return;
    if (mapPanMode && lv_event_get_target(event)==mapImage) return;
    auto input = lv_indev_active();
    if (!input) return;
    if (lv_obj_is_visible(pairingPanel)) return;
    if (deleteDialog && lv_obj_is_visible(deleteDialog)) return;
    if (mapStopDialog && lv_obj_is_visible(mapStopDialog)) return;
    const auto direction = lv_indev_get_gesture_dir(input);
    if (direction != LV_DIR_LEFT && direction != LV_DIR_RIGHT) return;
    lv_indev_wait_release(input);
    ++swipeCount;
    if (direction == LV_DIR_LEFT) showPage((currentPage + 1)%NORMAL_PAGES);
    if (direction == LV_DIR_RIGHT) showPage((currentPage + NORMAL_PAGES - 1)%NORMAL_PAGES);
    Serial.printf("HUD swipe: page=%d; count=%u\n", currentPage, swipeCount);
}

lv_obj_t* messageArea(lv_obj_t* parent, int y, int height, lv_obj_t*& title, lv_obj_t*& body) {
    auto area = box(parent, 10, y, 280, height, PANEL);
    lv_obj_set_scrollable(area, true);
    lv_obj_set_scroll_dir(area, LV_DIR_VER);
    lv_obj_set_style_pad_right(area, 5, 0);
    lv_obj_set_flex_flow(area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(area, 5, 0);
    title = label(area, 0, 0, 270, "");
    body = label(area, 0, 0, 270, "", MUTED);
    return area;
}

lv_obj_t* iconButton(lv_obj_t* parent,int x,int y,int width,const char* icon,lv_event_cb_t action,void* data=nullptr) {
    auto button=lv_button_create(parent);
    lv_obj_set_pos(button,x,y); lv_obj_set_size(button,width,30);
    lv_obj_set_style_bg_color(button,lv_color_hex(BUTTON),0);
    lv_obj_set_style_pad_all(button,0,0);
    auto caption=label(button,0,0,width,icon,CYAN,&lv_font_montserrat_18);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(caption);
    lv_obj_add_event_cb(button,action,LV_EVENT_CLICKED,data);
    return button;
}
void setMapPan(bool enabled) {
    mapPanMode=enabled; mapDragging=false;
    lv_obj_set_gesture_bubble(mapImage,!enabled);
    if(enabled) lv_obj_add_state(mapPanButton,LV_STATE_CHECKED);
    else lv_obj_remove_state(mapPanButton,LV_STATE_CHECKED);
    if(currentPage==2) lv_label_set_text(pageHeading,enabled ? "KÉO BẢN ĐỒ" : "BẢN ĐỒ");
}
void onMapControl(lv_event_t* event) {
    if(!mapLive) return;
    mapMenuAt=millis();
    const int action=reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
    if(action==5) {
        const bool enabled=!mapPanMode;
        if(navStreamControl(enabled ? 5 : 3)) setMapPan(enabled);
    } else if(navStreamControl(action) && action==3) setMapPan(false);
}
void onMapMenu(lv_event_t*) { mapMenuOpen=!mapMenuOpen; mapMenuAt=millis(); }
void cancelMapStop(lv_event_t*) { lv_obj_set_hidden(mapStopDialog,true); mapMenuAt=millis(); }
void confirmMapStop(lv_event_t*) {
    if(navStreamControl(6)) {
        mapStopPending=true; mapStopAt=millis(); mapMenuOpen=false;
        lv_obj_set_hidden(mapStopDialog,true);
    } else lv_label_set_text(mapStopMessage,"Chưa gửi được lệnh. Kiểm tra kết nối rồi thử lại.");
}
void askMapStop(lv_event_t*) {
    lv_label_set_text(mapStopMessage,"Kết thúc chuyến đi hiện tại?");
    lv_obj_set_hidden(mapStopDialog,false);
}
void updateMapChrome(uint32_t now) {
    if(!mapMenuButton || !mapSearchPanel) return;
    aiVoiceHistoryRead(mapHistoryState);
    if(mapHistoryAppliedRevision!=mapHistoryState.revision) {
        mapHistoryAppliedRevision=mapHistoryState.revision;
        mapTripActive=mapHistoryState.navigating;
        if(!mapTripActive && mapLive) clearMaps(true);
    }
    const bool modal=lv_obj_is_visible(mapSearchPanel) || (mapStopDialog && lv_obj_is_visible(mapStopDialog));
    if(mapMenuOpen && !mapPanMode && !mapDragging && uint32_t(now-mapMenuAt)>5000) mapMenuOpen=false;
    const bool controls=mapTripActive && mapMenuOpen && !modal;
    lv_obj_set_hidden(mapControls,!controls);
    lv_obj_set_hidden(mapMenuButton,!mapTripActive || modal);
    lv_label_set_text(lv_obj_get_child(mapMenuButton,0),mapMenuOpen ? LV_SYMBOL_CLOSE : LV_SYMBOL_LIST);
    lv_obj_set_hidden(speedBox,!mapLive || controls || modal);
    lv_obj_set_hidden(mapSearchButton,modal || !controls);
    lv_obj_set_pos(mapSearchButton,mapTripActive ? 172 : 262,mapTripActive ? 121 : 3);
    const bool home=!mapTripActive && !modal;
    lv_obj_set_hidden(mapHomeUi.panel,!home);
    if(home) {
        lv_obj_set_hidden(mapSource,true); lv_obj_set_hidden(lv_obj_get_parent(mapTitle),true);
        const char* items[5]; for(int i=0;i<5;++i) items[i]=mapHistoryState.items[i];
        mapHomeUi.show(mapHistoryState.count,items,mapHistoryState.revision,mapHistoryState.connected,mapHomeRecording);
    }
    if(mapStopPending && uint32_t(now-mapStopAt)>5000) {
        mapStopPending=false;
        lv_label_set_text(mapStopMessage,"Chưa xác nhận kết thúc. Bạn muốn thử gửi lại?");
        lv_obj_set_hidden(mapStopDialog,false);
    }
}
void onMapHomeMic(lv_event_t* event) {
    const auto code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) {
        mapHomeRecording=aiVoiceStart(1);
        if(!mapHomeRecording) lv_label_set_text(mapHomeUi.heading,"Mở HUD Connect rồi thử lại");
    } else if(code==LV_EVENT_RELEASED && mapHomeRecording) {
        aiVoiceStop(); mapHomeRecording=false;
        lv_obj_set_hidden(mapSearchPanel,false); mapSearchRevision=UINT32_MAX;
    } else if(code==LV_EVENT_PRESS_LOST && mapHomeRecording) {
        aiVoiceCancel(); mapHomeRecording=false;
    }
}
void onMapHistoryChoose(lv_event_t* event) {
    const auto index=uint8_t(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if(aiVoiceHistoryControl(1,index)) {
        lv_obj_set_hidden(mapSearchPanel,false); mapSearchRevision=UINT32_MAX;
    } else lv_label_set_text(mapHomeUi.heading,"Chưa sẵn sàng. Hãy thử lại sau.");
}
void closeMapSearch(lv_event_t*) {
    aiVoiceDestinationControl(0);
    if(aiVoiceBusy()) aiVoiceCancel();
    lv_obj_set_hidden(mapSearchPanel,true); mapSearchRevision=UINT32_MAX;
}
void onMapSearchMic(lv_event_t* event) {
    const auto code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) {
        if(aiVoiceStart(1)) lv_label_set_text(mapSearchStatus,"Đang chuẩn bị mic…");
        else lv_label_set_text(mapSearchStatus,"Chưa sẵn sàng. Kiểm tra HUD Connect.");
    } else if(code==LV_EVENT_RELEASED && aiVoiceBusy()) aiVoiceStop();
    else if(code==LV_EVENT_PRESS_LOST && aiVoiceBusy()) aiVoiceCancel();
}
void onMapSearchChoose(lv_event_t* event) {
    const int index=reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
    if(aiVoiceDestinationControl(1,index)) lv_label_set_text(mapSearchStatus,"Đang tính tuyến…");
    else lv_label_set_text(mapSearchStatus,"Chưa gửi được lựa chọn. Hãy thử lại.");
}
void onMapSearchStart(lv_event_t*) {
    if(aiVoiceDestinationControl(2)) { lv_obj_set_hidden(mapSearchPanel,true); mapSearchRevision=UINT32_MAX; }
}
void refreshMapSearch() {
    if(!mapSearchPanel || lv_obj_is_hidden(mapSearchPanel)) return;
    aiVoiceDestinationRead(mapSearchState);
    char voiceStatus[128]; aiVoiceReadStatus(voiceStatus);
    const char* message=mapSearchState.status[0] ? mapSearchState.status : voiceStatus;
    const char* items[5]; for(int i=0;i<5;++i) items[i]=mapSearchState.items[i];
    mapSearchUi.show(mapSearchState.phase,message[0] ? message : "Giữ mic rồi nói địa điểm",
        mapSearchState.count,items,lv_obj_get_width(mapSearchPanel),lv_obj_get_height(mapSearchPanel),mapSearchState.session);

}
void onMapTouch(lv_event_t* event) {
    if(!mapLive) return;
    if(lv_event_get_code(event)==LV_EVENT_CLICKED && !mapPanMode) { onMapMenu(nullptr); return; }
    if(!mapPanMode) return;
    mapMenuAt=millis();
    auto input=lv_indev_active();
    if(!input) return;
    const auto code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) { lv_indev_get_point(input,&mapDragStart); mapDragging=true; }
    else if(code==LV_EVENT_PRESS_LOST) mapDragging=false;
    else if(code==LV_EVENT_RELEASED && mapDragging) {
        mapDragging=false;
        lv_point_t point; lv_indev_get_point(input,&point);
        const int dx=constrain(point.x-mapDragStart.x,-300,300);
        const int dy=constrain(point.y-mapDragStart.y,-200,200);
        if(abs(dx)>=10 || abs(dy)>=10) {
            const int scale=viewHeight>viewWidth ? pageWidth : 300;
            navStreamControl(4,constrain(dx*300/scale,-300,300),constrain(dy*300/scale,-200,200));
        }
    }
}
void closeAi(lv_event_t*) { showPage(aiReturnPage); }

void executeAiTool(const AiToolRequest& request) {
    static char output[3072]; output[0]=0;
    bool ok=false;
    if(request.action==1 && request.value<=3) {
        const size_t limit=std::min(size_t(request.value),count);
        size_t used=snprintf(output,sizeof(output),"HUD có %u thông báo%s",unsigned(count),limit ? "; các mục mới nhất:\n" : ".");
        for(size_t i=0;i<limit;++i) {
            char when[40],line[1024];
            if(!sourceTime(notices[i],when,sizeof(when),true)) receivedTime(notices[i],when,sizeof(when),true);
            snprintf(line,sizeof(line),"%u. %s (%s)\n%s\n%s\n",unsigned(i+1),appName(notices[i].app),when,
                notices[i].title,notices[i].message[0] ? notices[i].message : "iPhone chưa chia sẻ nội dung.");
            size_t n=std::min(strlen(line),sizeof(output)-used-1);
            if(n<strlen(line)) while(n && (uint8_t(line[n])&0xc0)==0x80) --n;
            memcpy(output+used,line,n); used+=n; output[used]=0;
        }
        ok=true;
    } else if(request.action==2 && request.value==0) {
        if(musicAvailable) {
            snprintf(output,sizeof(output),"%s\n%s\n%s. Âm lượng: %s",
                lv_label_get_text(musicTitle),lv_label_get_text(musicArtist),
                musicPlaybackKnown ? (musicPlaying ? "Đang phát" : "Đang dừng") : "Chưa rõ trạng thái phát",
                lv_label_get_text(musicVolume)); ok=true;
        } else strlcpy(output,"Chưa có dữ liệu trình phát nhạc từ iPhone.",sizeof(output));
    } else if(request.action==3 && request.value<=6) {
        auto command=PhoneMusicCommand(request.value);
        if(!musicAvailable) strlcpy(output,"Chưa kết nối điều khiển nhạc. Mở trình phát trên iPhone trước.",sizeof(output));
        else if(musicPlaybackKnown && ((request.value==0 && musicPlaying) || (request.value==1 && !musicPlaying))) {
            strlcpy(output,musicPlaying ? "Nhạc đang phát rồi." : "Nhạc đang dừng rồi.",sizeof(output)); ok=true;
        } else {
            if(request.value<=1 && !(supportedMusicCommands&(1<<request.value)) && musicPlaybackKnown &&
               (supportedMusicCommands&(1<<unsigned(PhoneMusicCommand::Toggle)))) command=PhoneMusicCommand::Toggle;
            const char* names[]={"phát nhạc","dừng nhạc","phát/dừng nhạc","bài tiếp theo","bài trước","tăng âm lượng","giảm âm lượng"};
            if(!(supportedMusicCommands&(1<<unsigned(command)))) strlcpy(output,"Trình phát chưa hỗ trợ lệnh này.",sizeof(output));
            else if(phoneLinkMusicCommand(command)) { snprintf(output,sizeof(output),"Đã gửi lệnh %s tới iPhone.",names[request.value]); ok=true; }
            else strlcpy(output,"Chưa gửi được lệnh nhạc. Hãy thử lại.",sizeof(output));
        }
    } else if(request.action>=4 && request.action<=6) {
        auto settings=hudSettingsGet(); bool valid=true;
        if(request.action==4) { valid=request.value<8; settings.theme=request.value; }
        if(request.action==5) { valid=request.value<8; settings.face=request.value; }
        if(request.action==6) settings.timeoutSeconds=request.value;
        ok=valid && hudSettingsApply(settings);
        strlcpy(output,ok ? "Đã lưu và áp dụng cài đặt lên HUD." : "Cài đặt không hợp lệ hoặc chưa lưu được.",sizeof(output));
    } else if(request.action==7 && request.value<4) {
        showPage(request.value); ok=true; strlcpy(output,"Đã mở màn hình bạn chọn trên HUD.",sizeof(output));
    } else strlcpy(output,"HUD không hỗ trợ lệnh này.",sizeof(output));
    aiVoiceReplyTool(request,ok,output);
}

void renderAiMessage(size_t index,bool assistant,bool waiting) {
    auto& turn=aiTurns[index];
    const int row=index*2+(assistant ? 1 : 0);
    char* text=assistant ? turn.answer : turn.question;
    const size_t shown=assistant ? turn.shownAnswerBytes : strlen(text);
    const char saved=text[shown]; text[shown]=0;
    lv_label_set_text(aiBodies[row],text[0] ? text : (waiting ? "Đang trả lời..." : "Không có câu trả lời"));
    text[shown]=saved;
    lv_obj_update_layout(aiBodies[row]);
    const int height=lv_obj_get_height(aiBodies[row])+38;
    lv_obj_set_height(aiBubbles[row],height); lv_obj_set_height(aiRows[row],height);
}

void refreshAiChat(const AiVoiceView& view) {
    if(!aiTurns || !view.question[0]) return;
    const bool newTurn=!aiTurnCount || aiTurns[aiTurnCount-1].session!=view.session;
    const bool follow=newTurn || lv_obj_get_scroll_bottom(aiChatList)<30;
    if(newTurn) {
        if(aiTurnCount) aiTurns[aiTurnCount-1].shownAnswerBytes=strlen(aiTurns[aiTurnCount-1].answer);
        if(aiTurnCount==AI_TURNS) { memmove(aiTurns,aiTurns+1,sizeof(AiTurn)*(AI_TURNS-1)); --aiTurnCount; }
        aiTurns[aiTurnCount]={}; aiTurns[aiTurnCount++].session=view.session;
    }
    auto& turn=aiTurns[aiTurnCount-1];
    strlcpy(turn.question,view.question,sizeof(turn.question));
    if(strncmp(turn.answer,view.answer,turn.shownAnswerBytes)!=0) turn.shownAnswerBytes=0;
    strlcpy(turn.answer,view.answer,sizeof(turn.answer));
    const size_t length=strlen(turn.answer);
    if(turn.shownAnswerBytes>length) turn.shownAnswerBytes=0;
    if(length && !turn.shownAnswerBytes) {
        turn.shownAnswerBytes=hudvoice::revealUtf8(turn.answer,length,0,1); aiTypingAt=millis();
    }
    lv_obj_set_hidden(aiEmpty,true);
    for(int i=0;i<AI_TURNS*2;++i) {
        const size_t index=i/2; const bool assistant=i%2;
        lv_obj_set_hidden(aiRows[i],index>=aiTurnCount);
        if(index>=aiTurnCount) continue;
        renderAiMessage(index,assistant,view.busy && index==aiTurnCount-1);
    }
    lv_obj_update_layout(aiChatList);
    if(follow) lv_obj_scroll_to_y(aiChatList,LV_COORD_MAX,LV_ANIM_OFF);
}

void updateAiTyping(uint32_t now,const AiVoiceView& view) {
    if(!aiTurns || !aiTurnCount) return;
    auto& turn=aiTurns[aiTurnCount-1];
    const size_t length=strlen(turn.answer);
    if(turn.shownAnswerBytes<length && now-aiTypingAt>=20) {
        const bool follow=lv_obj_get_scroll_bottom(aiChatList)<30;
        const size_t steps=std::min(size_t(6),size_t((now-aiTypingAt)/20));
        turn.shownAnswerBytes=currentPage==AI_PAGE && turn.session==view.session ?
            hudvoice::revealUtf8(turn.answer,length,turn.shownAnswerBytes,steps) : length;
        aiTypingAt=now;
        renderAiMessage(aiTurnCount-1,true,false);
        lv_obj_update_layout(aiChatList);
        if(follow) lv_obj_scroll_to_y(aiChatList,LV_COORD_MAX,LV_ANIM_OFF);
    }
    const bool typing=turn.shownAnswerBytes<length;
    if(typing!=aiTypingStatus) {
        aiTypingStatus=typing;
        lv_label_set_text(aiStatus,typing ? "AI đang trả lời..." : view.status);
    }
}
}

bool companionUiTouch(bool down,int x,int y,bool wakeOnly) {
    if(!root || !aiListening) return false;
    // Keep the AI drawer reachable from destination results/confirmation.
    // Remember where this contact began so moving upwards does not suddenly
    // turn the same edge swipe into a blocked modal contact.
    static bool previousDown=false,mapSearchEdge=false;
    if(down && !previousDown) mapSearchEdge=mapSearchPanel && lv_obj_is_visible(mapSearchPanel) && y>=viewHeight-40;
    const bool blocked=wakeOnly || (pairingPanel && lv_obj_is_visible(pairingPanel)) ||
        (deleteDialog && lv_obj_is_visible(deleteDialog)) || (mapStopDialog && lv_obj_is_visible(mapStopDialog)) || (mapSearchPanel && lv_obj_is_visible(mapSearchPanel) && !mapSearchEdge);
    const bool consumed=aiGesture.consumes();
    const auto action=aiGesture.update(down,x,y,millis(),currentPage==AI_PAGE,!aiVoiceBusy(),blocked,viewHeight);
    previousDown=down;
    if(!down) mapSearchEdge=false;
    if(action!=hudvoice::Gesture::None) pendingAiGesture=action;
    // Input callbacks only classify/consume the contact. UI mutation and BLE
    // commands are applied on the next normal main-loop tick.
    return consumed || aiGesture.consumes() || action!=hudvoice::Gesture::None;
}

void applyPendingAiGesture() {
    const auto action=pendingAiGesture;
    pendingAiGesture=hudvoice::Gesture::None;
    if(action==hudvoice::Gesture::Open) { mapDragging=false; aiReturnPage=currentPage; showPage(AI_PAGE); }
    else if(action==hudvoice::Gesture::Close) showPage(aiReturnPage);
    else if(action==hudvoice::Gesture::Begin) {
        aiHolding=aiVoiceStart();
        lv_obj_set_hidden(aiListening,!aiHolding);
        if(!aiHolding) lv_label_set_text(aiStatus,"Kiểm tra mic và HUD Connect");
    } else if(action==hudvoice::Gesture::Send || action==hudvoice::Gesture::Cancel) {
        if(aiHolding) {
            if(action==hudvoice::Gesture::Cancel) aiVoiceCancel(); else aiVoiceStop();
        }
        aiHolding=false; lv_obj_set_hidden(aiListening,true);
    }
}

void companionUiBegin() {
    const bool fontOk = fontRendersGlyph('A') && fontRendersGlyph(0x0111) &&
                        fontRendersGlyph(0x1ED9);
    if (!fontOk) textFont = &lv_font_montserrat_18;
    Serial.println(fontOk ? "Vietnamese font render check passed (A, d-stroke, o-circumflex-dot)" :
                   "Vietnamese font failed; using built-in fallback");
    const bool homeFontsOk = fontRendersGlyph('8', &hud_font_clock_80) &&
                             fontRendersGlyph(0x1EE9, &hud_font_vietnamese_24);
    homeFontsReady = homeFontsOk;
    lv_point_t clockSize, dateSize;
    lv_text_get_size(&clockSize, "88:88", &hud_font_clock_80, 0, 0, 1000, LV_TEXT_FLAG_NONE);
    lv_text_get_size(&dateSize, "Chủ nhật, 31/10/2026", &hud_font_vietnamese_24, 0, 0, 1000, LV_TEXT_FLAG_NONE);
    Serial.printf("Home fonts: render=%s; clock=%dx%d; date=%dx%d (panel 300x200)\n",
                  homeFontsOk ? "OK" : "FAILED", int(clockSize.x), int(clockSize.y), int(dateSize.x), int(dateSize.y));
    root = box(nullptr, 0, 0, 320, 240, BG);
    lv_obj_set_gesture_bubble(root, false);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_add_event_cb(root, onGesture, LV_EVENT_GESTURE, nullptr);
    pageHeading = label(root, 12, 4, 155, "ĐỒNG HỒ", CYAN);
    for (int i = 0; i < PAGE_COUNT; ++i) {
        pageDots[i] = box(root, 174 + i * 10, 15, 4, 4, MUTED);
        lv_obj_set_style_radius(pageDots[i], LV_RADIUS_CIRCLE, 0);
    }
    smallClock=label(root,220,5,72,"--:--",WHITE,&lv_font_montserrat_18);
    lv_obj_set_style_text_align(smallClock,LV_TEXT_ALIGN_RIGHT,0);
    statusDot = box(root, 303, 12, 7, 7, MUTED);
    lv_obj_set_style_radius(statusDot, LV_RADIUS_CIRCLE, 0);

    for (int i = 0; i < PAGE_COUNT; ++i) pages[i] = box(root, 10, 32, 300, 200, PANEL);
    clockLabel = label(pages[0], 0, 12, 300, "--:--", WHITE, &hud_font_clock_80);
    lv_obj_set_style_text_align(clockLabel, LV_TEXT_ALIGN_CENTER, 0);
    dateLabel = label(pages[0], 0, 94, 300, "Chưa đồng bộ giờ", CYAN, &hud_font_vietnamese_24);
    lv_obj_set_style_text_align(dateLabel, LV_TEXT_ALIGN_CENTER, 0);
    extraLabel = label(pages[0], 0, 104, 300, "--", CYAN, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(extraLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_hidden(extraLabel, true);
    lunarLabel=label(pages[0],0,174,300,"Âm lịch: --/--",MUTED);
    lv_obj_set_style_text_align(lunarLabel,LV_TEXT_ALIGN_CENTER,0);
    analog = box(pages[0], 8, 20, 144, 144, BG);
    lv_obj_set_style_radius(analog, LV_RADIUS_CIRCLE, 0);
    for (int i = 0; i < 12; ++i) {
        const float angle = i * 3.14159265f / 6;
        auto dot = box(analog, 70 + int(61 * sinf(angle)), 70 - int(61 * cosf(angle)), 4, 4, MUTED);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    }
    for (int i = 0; i < 3; ++i) {
        hands[i] = lv_line_create(analog);
        lv_obj_set_style_line_width(hands[i], i == 2 ? 2 : 4, 0);
        lv_obj_set_style_line_rounded(hands[i], true, 0);
    }
    lv_obj_set_hidden(analog, true);

    // Faces 6 and 7 use the same validated GPS stream as the Maps overlay.
    // They are separate containers so switching faces never leaves an old
    // speed value or needle visible underneath the selected design.
    speedDigital = box(pages[0], 0, 0, 300, 200, PANEL);
    label(speedDigital, 0, 8, 300, "TỐC ĐỘ GPS", CYAN, &hud_font_vietnamese_14);
    speedDigitalValue = label(speedDigital, 0, 28, 214, "---", WHITE, &hud_font_clock_80);
    lv_obj_set_style_text_align(speedDigitalValue, LV_TEXT_ALIGN_RIGHT, 0);
    label(speedDigital, 220, 82, 72, "km/h", CYAN, &lv_font_montserrat_18);
    speedDigitalStatus = label(speedDigital, 0, 121, 300, "GPS chưa có dữ liệu", MUTED, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(speedDigitalStatus, LV_TEXT_ALIGN_CENTER, 0);
    speedDigitalClock = label(speedDigital, 0, 148, 300, "--:--", CYAN, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(speedDigitalClock, LV_TEXT_ALIGN_CENTER, 0);
    speedDigitalDate = label(speedDigital, 0, 181, 300, "Chưa đồng bộ giờ", MUTED, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(speedDigitalDate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_hidden(speedDigital, true);

    speedDial = box(pages[0], 50, 0, 200, 200, BG);
    lv_obj_set_style_radius(speedDial, LV_RADIUS_CIRCLE, 0);
    constexpr float pi = 3.14159265f;
    for (int i = 0; i <= 16; ++i) {
        const float angle = (135.0f + 270.0f * i / 16) * pi / 180.0f;
        const int length = (i % 4 == 0) ? 10 : 6;
        const int x = 100 + int(82 * cosf(angle));
        const int y = 100 + int(82 * sinf(angle));
        auto tick = box(speedDial, x - 2, y - 2, length, 3, i % 4 == 0 ? WHITE : MUTED);
        lv_obj_set_style_transform_rotation(tick, int((1350 + 2700 * i / 16) % 3600), 0);
    }
    const int values[] = {0, 40, 80, 120, 160};
    for (int i = 0; i < 5; ++i) {
        const float angle = (135.0f + 270.0f * i / 4) * pi / 180.0f;
        auto value = label(speedDial, 100 + int(62 * cosf(angle)) - 17, 96 + int(62 * sinf(angle)) - 8,
                           34, "", MUTED, &lv_font_montserrat_12);
        lv_label_set_text_fmt(value, "%d", values[i]);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
    }
    speedDialNeedle = lv_line_create(speedDial);
    lv_obj_set_style_line_width(speedDialNeedle, 4, 0);
    lv_obj_set_style_line_rounded(speedDialNeedle, true, 0);
    speedDialValue = label(speedDial, 0, 74, 200, "---", WHITE, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(speedDialValue, LV_TEXT_ALIGN_CENTER, 0);
    label(speedDial, 0, 107, 200, "km/h", MUTED, &lv_font_montserrat_12);
    speedDialClock = label(speedDial, 0, 132, 200, "--:--", CYAN, &lv_font_montserrat_18);
    lv_obj_set_style_text_align(speedDialClock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_hidden(speedDial, true);

    inboxCount=label(pages[1],8,2,205,"0 thông báo",MUTED);
    refreshButton=iconButton(pages[1],220,0,34,LV_SYMBOL_REFRESH,onRefresh);
    refreshCaption=lv_obj_get_child(refreshButton,0);
    inboxDeleteAll=iconButton(pages[1],262,0,34,LV_SYMBOL_TRASH,askDelete,reinterpret_cast<void*>(1));
    inboxList=box(pages[1],0,34,300,166,PANEL);
    lv_obj_set_scrollable(inboxList,true); lv_obj_set_scroll_dir(inboxList,LV_DIR_VER);
    for (int i=0;i<10;++i) {
        auto row=box(inboxList,4,i*82,287,77,BUTTON); inboxRows[i]=row;
        lv_obj_set_clickable(row,true);
        lv_obj_add_event_cb(row,openNotice,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(i)));
        rowApps[i]=label(row,7,3,195,"",CYAN,&hud_font_vietnamese_14);
        lv_label_set_long_mode(rowApps[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rowApps[i],20);
        rowTimes[i]=label(row,208,5,70,"",MUTED,&lv_font_montserrat_12);
        lv_obj_set_style_text_align(rowTimes[i],LV_TEXT_ALIGN_RIGHT,0);
        rowTitles[i]=label(row,7,23,271,"",WHITE,textFont);
        lv_label_set_long_mode(rowTitles[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rowTitles[i],24);
        rowContents[i]=label(row,7,48,271,"",MUTED,textFont);
        lv_label_set_long_mode(rowContents[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rowContents[i],24);
        lv_obj_set_hidden(row,true);
    }
    inboxDetail=box(pages[1],0,0,300,200,PANEL);
    iconButton(inboxDetail,0,0,34,LV_SYMBOL_LEFT,backToList);
    inboxApp=label(inboxDetail,42,1,211,"",CYAN);
    lv_label_set_long_mode(inboxApp,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(inboxApp,28);
    detailDelete=iconButton(inboxDetail,262,0,34,LV_SYMBOL_TRASH,askDelete);
    inboxTime=label(inboxDetail,10,33,280,"",MUTED);
    messageArea(inboxDetail,87,113,inboxTitle,inboxBody);
    lv_obj_set_style_text_font(inboxTitle,&hud_font_vietnamese_24,0);
    lv_obj_set_hidden(inboxDetail,true);
    deleteDialog=box(pages[1],14,22,272,158,OVERLAY);
    deletePrompt=label(deleteDialog,12,8,248,"",WHITE);
    iconButton(deleteDialog,62,117,52,LV_SYMBOL_CLOSE,cancelDelete);
    iconButton(deleteDialog,158,117,52,LV_SYMBOL_OK,confirmDelete);
    lv_obj_set_hidden(deleteDialog,true);

    mapSource = label(pages[2], 10, 3, 280, "MAPS", CYAN, &lv_font_montserrat_14);
    messageArea(pages[2], 26, 174, mapTitle, mapBody);
    lv_obj_set_style_text_font(mapTitle, &hud_font_vietnamese_24, 0);
    lv_obj_set_style_text_font(mapBody, &hud_font_vietnamese_24, 0);
    lv_obj_set_style_text_color(mapBody, lv_color_hex(WHITE), 0);
    clearMaps();
    mapImage = lv_image_create(pages[2]);
    lv_obj_set_pos(mapImage, 0, 0);
    lv_obj_set_size(mapImage, 300, 200);
    lv_obj_set_clickable(mapImage, true);
    lv_obj_set_press_lock(mapImage, true);
    lv_obj_set_gesture_bubble(mapImage,true);
    lv_obj_add_event_cb(mapImage,onMapTouch,LV_EVENT_ALL,nullptr);
    lv_obj_set_hidden(mapImage, true);
    googleOverlay = box(pages[2], 4, 4, 292, 60, 0x080E18);
    lv_obj_set_style_bg_opa(googleOverlay, LV_OPA_90, 0);
    googleHeading = label(googleOverlay, 6, 2, 280, "Google Maps", WHITE, &hud_font_vietnamese_24);
    lv_label_set_long_mode(googleHeading, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(googleHeading, 31);
    googleText = label(googleOverlay, 6, 32, 280, "", WHITE);
    lv_label_set_long_mode(googleText, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(googleText, 26);
    lv_obj_set_hidden(googleOverlay, true);
    speedBox=box(pages[2],152,164,104,30,OVERLAY);
    lv_obj_set_style_bg_opa(speedBox,LV_OPA_90,0);
    speedLabel=label(speedBox,4,5,96,"--- km/h",WHITE,&lv_font_montserrat_18);
    lv_obj_set_style_text_align(speedLabel,LV_TEXT_ALIGN_CENTER,0);
    mapControls=box(pages[2],8,118,246,38,OVERLAY);
    lv_obj_set_style_bg_opa(mapControls,LV_OPA_90,0);
    iconButton(mapControls,4,4,34,LV_SYMBOL_PLUS,onMapControl,reinterpret_cast<void*>(1));
    iconButton(mapControls,44,4,34,LV_SYMBOL_MINUS,onMapControl,reinterpret_cast<void*>(2));
    mapPanButton=iconButton(mapControls,84,4,34,"",onMapControl,reinterpret_cast<void*>(5));
    lv_obj_set_style_border_width(mapPanButton,2,LV_STATE_CHECKED);
    lv_obj_set_style_border_color(mapPanButton,lv_color_hex(CYAN),LV_STATE_CHECKED);
    label(mapPanButton,12,0,12,LV_SYMBOL_UP,CYAN,&lv_font_montserrat_10);
    label(mapPanButton,12,18,12,LV_SYMBOL_DOWN,CYAN,&lv_font_montserrat_10);
    label(mapPanButton,3,9,12,LV_SYMBOL_LEFT,CYAN,&lv_font_montserrat_10);
    label(mapPanButton,23,9,12,LV_SYMBOL_RIGHT,CYAN,&lv_font_montserrat_10);
    iconButton(mapControls,124,4,34,LV_SYMBOL_GPS,onMapControl,reinterpret_cast<void*>(3));
    iconButton(mapControls,204,4,34,LV_SYMBOL_STOP,askMapStop);
    lv_obj_set_hidden(mapControls,true);
    mapSearchButton=HudDestinationUi::searchButton(pages[2],262,3,BUTTON,CYAN,[](lv_event_t*) {
        mapMenuOpen=false;
        mapSearchRevision=UINT32_MAX; lv_label_set_text(mapSearchStatus,"Giữ mic rồi nói nơi muốn đến");
        lv_obj_set_hidden(mapSearchPanel,false);
    });
    mapMenuButton=iconButton(pages[2],262,160,34,LV_SYMBOL_LIST,onMapMenu);
    lv_obj_set_hidden(mapMenuButton,true);
    mapPortraitHint=label(pages[2],8,156,204,"Thu phóng: + / -\nKéo: bật nút bốn mũi tên.",MUTED,&hud_font_vietnamese_14);
    lv_obj_set_hidden(mapPortraitHint,true);
    mapHomeUi.create(pages[2],pageWidth,pageHeight,PANEL,BUTTON,CYAN,WHITE,MUTED,BG,
        textFont,&hud_font_vietnamese_14,onMapHomeMic,onMapHistoryChoose);
    mapSearchUi.create(pages[2],pageWidth,pageHeight,PANEL,BUTTON,CYAN,WHITE,MUTED,BG,
        textFont,&hud_font_vietnamese_14,onMapSearchMic,onMapSearchChoose,onMapSearchStart,closeMapSearch);
    mapSearchPanel=mapSearchUi.panel; mapSearchStatus=mapSearchUi.status;
    mapSearchMic=mapSearchUi.mic; mapSearchStart=mapSearchUi.start; mapSearchCancel=mapSearchUi.cancel;
    lv_obj_set_hidden(mapSearchPanel,true);
    mapStopDialog=HudDestinationUi::stopConfirmation(pages[2],pageWidth,OVERLAY,BUTTON,WHITE,
        textFont,cancelMapStop,confirmMapStop);
    mapStopMessage=lv_obj_get_child(mapStopDialog,0); lv_obj_set_hidden(mapStopDialog,true);

    musicTitle = label(pages[3], 10, 14, 280, "Mở nhạc trên iPhone", WHITE, &hud_font_vietnamese_24);
    lv_label_set_long_mode(musicTitle, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    musicArtist = label(pages[3], 10, 54, 280, "", CYAN);
    lv_label_set_long_mode(musicArtist, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    musicStatus = label(pages[3], 10, 94, 185, "", MUTED);
    lv_label_set_long_mode(musicStatus, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(musicStatus, 24);
    musicVolume = label(pages[3], 202, 96, 90, LV_SYMBOL_VOLUME_MID " --%", CYAN, &lv_font_montserrat_18);
    const char* controls[] = {LV_SYMBOL_MINUS, LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT, LV_SYMBOL_PLUS};
    for (int i = 0; i < MUSIC_BUTTON_COUNT; ++i) {
        musicButtons[i] = lv_button_create(pages[3]);
        lv_obj_set_pos(musicButtons[i], 10 + i * 58, 142);
        lv_obj_set_size(musicButtons[i], 48, 48);
        lv_obj_set_style_bg_color(musicButtons[i], lv_color_hex(0x214358), 0);
        lv_obj_set_style_pad_all(musicButtons[i], 0, 0);
        auto icon = label(musicButtons[i], 0, 0, 40, controls[i], CYAN, &lv_font_montserrat_24);
        lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(icon);
        if (i == 2) musicPlayIcon = icon;
        lv_obj_add_event_cb(musicButtons[i], onMusicCommand, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(intptr_t(i)));
    }
    resetMusic();

    aiStatus=label(pages[4],8,2,284,"Kết nối HUD Connect để hỏi AI",CYAN,&hud_font_vietnamese_14);
    lv_label_set_long_mode(aiStatus,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(aiStatus,22);
    lv_obj_set_width(aiStatus,253);
    aiCloseButton=iconButton(pages[4],267,0,30,LV_SYMBOL_DOWN,closeAi);
    lv_obj_set_height(aiCloseButton,24);
    aiTurns=static_cast<AiTurn*>(heap_caps_calloc(AI_TURNS,sizeof(AiTurn),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    aiChatList=box(pages[4],2,28,296,149,PANEL);
    lv_obj_set_scrollable(aiChatList,true); lv_obj_set_scroll_dir(aiChatList,LV_DIR_VER);
    lv_obj_set_flex_flow(aiChatList,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(aiChatList,9,0);
    aiEmpty=label(aiChatList,0,0,284,"Bạn muốn hỏi gì?\n\nGiữ trên màn hình để nói.\nThả tay để gửi.",MUTED);
    for(int i=0;i<AI_TURNS*2;++i) {
        const bool assistant=i%2;
        aiRows[i]=box(aiChatList,0,0,288,70,PANEL);
        aiBubbles[i]=box(aiRows[i],assistant ? 0 : 24,0,assistant ? 276 : 264,70,assistant ? BG : BUTTON);
        label(aiBubbles[i],10,5,240,assistant ? "AI" : "Bạn",CYAN,&hud_font_vietnamese_14);
        aiBodies[i]=label(aiBubbles[i],10,28,assistant ? 256 : 244,"",WHITE);
        lv_obj_set_hidden(aiRows[i],true);
    }
    aiHint=label(pages[4],8,180,286,"Giữ để nói - kéo xuống để hủy",MUTED,&hud_font_vietnamese_14);
    aiListening=box(pages[4],0,26,300,174,OVERLAY);
    aiPulse=box(aiListening,90,8,120,120,CYAN);
    lv_obj_set_style_radius(aiPulse,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_opa(aiPulse,LV_OPA_20,0);
    lv_obj_set_style_border_width(aiPulse,2,0); lv_obj_set_style_border_color(aiPulse,lv_color_hex(CYAN),0);
    aiListenCircle=box(aiListening,106,24,88,88,CYAN);
    lv_obj_set_style_radius(aiListenCircle,LV_RADIUS_CIRCLE,0);
    aiListenText=label(aiListenCircle,0,18,88,"Chuẩn\nbị...",BG);
    lv_obj_set_style_text_align(aiListenText,LV_TEXT_ALIGN_CENTER,0);
    aiListeningHint=label(aiListening,8,130,284,"Thả để gửi\nKéo xuống để hủy",WHITE,&hud_font_vietnamese_14);
    lv_obj_set_style_text_align(aiListeningHint,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_hidden(aiListening,true);

    noticePopup = box(lv_layer_top(), 10, 133, 300, 92, 0x173347);
    lv_obj_set_style_border_width(noticePopup, 1, 0);
    lv_obj_set_style_border_color(noticePopup, lv_color_hex(CYAN), 0);
    lv_obj_set_clickable(noticePopup, true);
    lv_obj_set_event_bubble(noticePopup, false);
    lv_obj_add_event_cb(noticePopup, onPopup, LV_EVENT_CLICKED, nullptr);
    popupApp = label(noticePopup, 9, 3, 242, "", CYAN);
    lv_label_set_long_mode(popupApp, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(popupApp, 24);
    popupText = label(noticePopup, 9, 31, 280, "", WHITE);
    lv_label_set_long_mode(popupText, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(popupText, 48);
    lv_obj_set_style_text_line_space(popupText, 0, 0);
    auto close = lv_button_create(noticePopup);
    popupCloseButton=close;
    lv_obj_set_pos(close, 260, 0);
    lv_obj_set_size(close, 36, 30);
    lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(close, 0, 0);
    lv_obj_set_event_bubble(close, false);
    auto closeIcon = label(close, 0, 0, 20, LV_SYMBOL_CLOSE, MUTED, &lv_font_montserrat_18);
    lv_obj_center(closeIcon);
    lv_obj_add_event_cb(close, onPopupClose, LV_EVENT_CLICKED, nullptr);
    hidePopup();
    pairingPanel = box(lv_layer_top(), 12, 38, 296, 154, 0x173347);
    lv_obj_set_style_border_width(pairingPanel, 2, 0);
    lv_obj_set_style_border_color(pairingPanel, lv_color_hex(CYAN), 0);
    label(pairingPanel, 12, 5, 270, "Nhập mã này trên iPhone", CYAN);
    pairingCode = label(pairingPanel, 0, 40, 292, "000000", WHITE, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(pairingCode, LV_TEXT_ALIGN_CENTER, 0);
    label(pairingPanel, 12, 91, 270, "Sau đó cho phép chia sẻ thông báo hệ thống.", MUTED);
    lv_obj_set_hidden(pairingPanel, true);
    refreshInbox();
    showPage(0);
    lv_screen_load(root);
    applySettings();
}

void companionUiLayout(int,int) {
    // A 180-degree flip keeps the existing 320x240 layout unchanged.
    aiGesture=hudvoice::AiGesture{}; pendingAiGesture=hudvoice::Gesture::None;
    aiHolding=false; if(aiListening) lv_obj_set_hidden(aiListening,true);
    if(root) lv_obj_invalidate(root);
}

void companionUiSetTime(const phone::ClockSample& sample, bool) {
    if (sample.utcSeconds < 1577836800LL || sample.utcSeconds > 4102444800LL ||
        sample.utcOffsetSeconds < -12 * 3600 || sample.utcOffsetSeconds > 16 * 3600) return;
    timeval value{};
    value.tv_sec = sample.utcSeconds;
    settimeofday(&value, nullptr);
    utcOffset = sample.utcOffsetSeconds;
    clockValid = true;
    renderedSecond = -1;
    refreshInbox();
}

void companionUiHandle(const PhoneEvent& event) {
    switch (event.type) {
    case PhoneEventType::Connected:
        lv_obj_set_hidden(deleteDialog,true);
        hiddenCount=0; detailOpen=false;
        hasGoogleGuide = false;
        hidePopup();
        resetMusic();
        feedbackUntil = 0;
        setRefreshBusy(false);
        count = selected = 0;
        refreshInbox();
        clearMaps(true);
        status(0x69E4AA);
        break;
    case PhoneEventType::PairingCode:
        hidePopup();
        lv_label_set_text_fmt(pairingCode, "%06u", unsigned(event.value));
        lv_obj_set_hidden(pairingPanel, false);
        break;
    case PhoneEventType::Secured:
        status(0x69E4AA);
        lv_obj_set_hidden(pairingPanel, true);
        break;
    case PhoneEventType::NotificationsReady:
        hudSettingsRequestAppNames();
        status(0x69E4AA);
        break;
    case PhoneEventType::NotificationsUnavailable:
        feedback("Bật chia sẻ");
        break;
    case PhoneEventType::RefreshStarted:
        feedbackUntil = 0;
        setRefreshBusy(true);
        break;
    case PhoneEventType::InboxReset:
        lv_obj_set_hidden(deleteDialog,true);
        hiddenCount=0; detailOpen=false;
        hasGoogleGuide = false;
        hidePopup();
        memset(notices, 0, sizeof(notices));
        count = selected = 0;
        refreshInbox();
        clearMaps();
        break;
    case PhoneEventType::RefreshFinished:
        setRefreshBusy(false);
        feedback(event.value ? "Đã yêu cầu" : "Thử lại sau");
        break;
    case PhoneEventType::Clock:
        companionUiSetTime(event.clock, true);
        break;
    case PhoneEventType::Notice: {
        bool hidden=false;
        for(size_t i=0;i<hiddenCount;++i) if(hiddenUids[i]==event.notice.uid) hidden=true;
        const auto fingerprint=noticeFingerprint(event.notice);
        for(size_t i=0;i<hiddenFingerprintCount;++i) if(fingerprint && hiddenFingerprints[i]==fingerprint) hidden=true;
        if(hidden) break;
        if (!hudSettingsAllowNotification(event.notice.app)) break;
        if (phone::isGoogleMaps(event.notice.app)) {
            googleGuide = event.notice; hasGoogleGuide = true;
            googleGuideAt = event.notice.preExisting ? millis() - 120001 : millis();
        }
        // ANCS UIDs have session scope; modified notifications replace their old row.
        size_t existing = count;
        for (size_t i = 0; i < count; ++i) if (notices[i].uid == event.notice.uid) existing = i;
        InboxEntry entry{};
        if(existing<count) entry=notices[existing];
        else { entry.receivedMillis=millis(); entry.receivedUnix=clockValid ? time(nullptr) : 0; }
        static_cast<phone::Notice&>(entry)=event.notice;
        if (existing < count) {
            for (size_t i = existing; i > 0; --i) notices[i] = notices[i - 1];
            notices[0] = entry;
            selected = 0;
        } else {
            const size_t last = count;
            for (size_t i = last; i > 0; --i) notices[i] = notices[i - 1];
            notices[0] = entry;
            ++count;
            selected = 0;
        }
        refreshInbox();
        showNoticePopup(event.notice);
        if (isNavigationApp(event.notice.app) && !mapLive) {
            showNavigationNotice(event.notice);
        }
        break;
    }
    case PhoneEventType::Removed:
        if(deleteUid==event.value) lv_obj_set_hidden(deleteDialog,true);
        if (hasGoogleGuide && event.value == googleGuide.uid) hasGoogleGuide = false;
        if (popupUntil && popupUid == event.value) hidePopup();
        for (size_t i = 0; i < count; ++i) {
            if (notices[i].uid != event.value) continue;
            for (size_t j = i + 1; j < count; ++j) notices[j - 1] = notices[j];
            --count;
            break;
        }
        refreshInbox();
        if (hasNavigationNotice && event.value == lastNavigationUid) clearMaps();
        break;
    case PhoneEventType::NoticeActionFailed:
        for(size_t i=0;i<count;++i) if(notices[i].uid==event.value) notices[i].deleting=false;
        feedback("iPhone chưa xóa được"); refreshInbox();
        break;
    case PhoneEventType::AppName:
        hudSettingsSetAppName(event.notice.app,event.notice.title);
        refreshInbox();
        for(size_t i=0;i<count;++i) if(notices[i].uid==popupUid) lv_label_set_text(popupApp,appName(notices[i].app));
        if(hasNavigationNotice && !strcmp(navigationNotice.app,event.notice.app)) lv_label_set_text(mapSource,appName(event.notice.app));
        break;
    case PhoneEventType::MusicReady:
        musicAvailable = true;
        lv_label_set_text(musicStatus, "Điều khiển nhạc iPhone");
        updateMusicControls();
        break;
    case PhoneEventType::MusicUnavailable:
        resetMusic();
        break;
    case PhoneEventType::MusicCommands:
        supportedMusicCommands = event.value;
        updateMusicControls();
        break;
    case PhoneEventType::MusicUpdate:
        switch (event.music.field) {
        case phone::MusicField::Title:
            lv_label_set_text(musicTitle, event.music.text[0] ? event.music.text : "Chưa có bài nhạc");
            break;
        case phone::MusicField::Artist:
            lv_label_set_text(musicArtist, event.music.text);
            break;
        case phone::MusicField::Playback:
            if (event.music.text[0] >= '0' && event.music.text[0] <= '3') {
                musicPlaying = event.music.text[0] != '0'; musicPlaybackKnown=true;
                musicFeedbackUntil = 0;
                lv_label_set_text(musicStatus, musicPlaying ? "Đang phát" : "Đã tạm dừng");
                updateMusicControls();
            }
            break;
        case phone::MusicField::Volume: {
            char* end;
            const float volume = strtof(event.music.text, &end);
            if (end != event.music.text && *end == '\0' && volume >= 0 && volume <= 1) {
                musicVolumePercent = int(volume * 100 + 0.5f);
                lv_label_set_text_fmt(musicVolume, LV_SYMBOL_VOLUME_MID " %d%%", musicVolumePercent);
            }
            break;
        }
        case phone::MusicField::Player:
            break;
        }
        break;
    case PhoneEventType::MusicCommandFailed:
        musicHint("iPhone chưa nhận lệnh");
        break;
    case PhoneEventType::Disconnected:
        lv_obj_set_hidden(deleteDialog,true);
        hiddenCount=0; detailOpen=false;
        hasGoogleGuide = false;
        hidePopup();
        resetMusic();
        // Apple explicitly invalidates notification data when the session ends.
        memset(notices, 0, sizeof(notices));
        feedbackUntil = 0;
        setRefreshBusy(false);
        count = selected = 0;
        refreshInbox();
        clearMaps(true);
        lv_obj_set_hidden(pairingPanel, true);
        status(MUTED);
        break;
    }
}

void updateSpeedFaces(uint32_t now) {
    const auto face = hudSettingsGet().face;
    if (face != SPEED_DIGITAL_FACE && face != SPEED_DIAL_FACE) return;
    static uint32_t lastRender = 0;
    if (now - lastRender < 50) return; // Smooth needle without redrawing on every LVGL loop.
    lastRender = now;
    const int tenths = navStreamSpeedTenths();
    const bool valid = tenths >= 0;
    const int rounded = valid ? (tenths + 5) / 10 : 0;
    if (face == SPEED_DIGITAL_FACE) {
        if (valid) {
            lv_label_set_text_fmt(speedDigitalValue, "%d", rounded);
            lv_label_set_text(speedDigitalStatus, rounded == 0 ? "GPS hợp lệ · đang đứng yên" : "GPS · cập nhật trực tiếp");
        } else {
            lv_label_set_text(speedDigitalValue, "---");
            lv_label_set_text(speedDigitalStatus, "GPS/Bluetooth không có dữ liệu mới");
        }
        return;
    }
    if (!valid) {
        speedDialDisplayed = -1;
        lv_label_set_text(speedDialValue, "---");
        lv_obj_set_hidden(speedDialNeedle, true);
        return;
    }
    const float target = std::min(160.0f, tenths / 10.0f);
    if (speedDialDisplayed < 0) speedDialDisplayed = target;
    const float elapsed = speedDialUpdatedAt ? std::min(0.25f, (now - speedDialUpdatedAt) / 1000.0f) : 0.0f;
    speedDialUpdatedAt = now;
    const float maximumStep = std::max(1.0f, elapsed * 45.0f);
    speedDialDisplayed += std::max(-maximumStep, std::min(maximumStep, target - speedDialDisplayed));
    const float angle = (135.0f + 270.0f * speedDialDisplayed / 160.0f) * 3.14159265f / 180.0f;
    speedDialNeedlePoints[0] = {100, 100};
    speedDialNeedlePoints[1] = {100 + int(57 * cosf(angle)), 100 + int(57 * sinf(angle))};
    lv_line_set_points(speedDialNeedle, speedDialNeedlePoints, 2);
    lv_obj_set_hidden(speedDialNeedle, false);
    lv_label_set_text_fmt(speedDialValue, "%d", rounded);
}

void companionUiTick() {
    const uint32_t now = millis();
    applyPendingAiGesture();
    AiToolRequest tool{};
    if(aiVoicePollTool(tool)) executeAiTool(tool);
    aiVoiceToolTick();
    static AiVoiceView aiView{};
    aiVoiceRead(aiView);
    if(aiHolding) hidePopup();
    if(aiStatus && aiRevision!=aiView.revision) {
        aiRevision=aiView.revision;
        lv_label_set_text(aiStatus,aiTypingStatus ? "AI đang trả lời..." : aiView.status);
        refreshAiChat(aiView);
        lv_label_set_text(aiListenText,aiView.recording ? "Đang\nnghe" : "Chuẩn\nbị...");
        if(aiHolding && !aiView.busy) { aiHolding=false; lv_obj_set_hidden(aiListening,true); }
    }
    updateAiTyping(now,aiView);
    static uint32_t lastPulse=0;
    if(aiHolding && now-lastPulse>=50) {
        lastPulse=now;
        const float pulse=(sinf(now*0.007f)+1)*0.5f;
        const int size=104+int(24*pulse);
        lv_obj_set_size(aiPulse,size,size); lv_obj_set_pos(aiPulse,(pageWidth-size)/2,68-size/2);
        lv_obj_set_style_border_opa(aiPulse,80+int(175*pulse),0);
    }
    if (appliedRevision != hudSettingsRevision()) {
        applySettings(); refreshInbox();
        bool popupStillAllowed = false;
        for (size_t i = 0; i < count; ++i) if (notices[i].uid == popupUid) popupStillAllowed = true;
        if (!popupStillAllowed) hidePopup();
    }
    const uint8_t* jpeg = nullptr;
    size_t jpegSize = 0;
    if (navStreamTake(jpeg, jpegSize) && !jpeg) { mapTripActive=false; clearMaps(true); }
    else if (jpegSize && jpeg) {
        if (!mapLive && currentPage!=AI_PAGE) showPage(2);
        lv_image_cache_drop(&mapFrame);
        mapFrame = {};
        mapFrame.header.magic = LV_IMAGE_HEADER_MAGIC;
        mapFrame.header.cf = LV_COLOR_FORMAT_RAW;
        mapFrame.header.w = 300;
        mapFrame.header.h = 200;
        mapFrame.data = jpeg;
        mapFrame.data_size = jpegSize;
        lv_image_set_src(mapImage, &mapFrame);
        lv_obj_set_hidden(mapImage, false);
        lv_obj_set_hidden(mapControls, false);
        mapLive = true;
        mapTripActive = true;
        lv_obj_set_hidden(mapSource,true); lv_obj_set_hidden(lv_obj_get_parent(mapTitle),true);
        lv_obj_set_hidden(mapPortraitHint,viewHeight<=viewWidth);
        mapReceivedAt = now;
    }
    if (mapLive && now - mapReceivedAt > 15000) {
        clearMaps(true);
        lv_label_set_text(mapTitle, "Dữ liệu bản đồ đã cũ");
        lv_label_set_text(mapBody, "Kiểm tra app HUD Connect và kết nối Bluetooth trên iPhone.");
    }
    updateGoogleOverlay();
    aiVoiceDestinationRead(mapSearchState);
    // Bring search results in front of the AI drawer once, even when the
    // destination panel remained open underneath that drawer.
    if(mapSearchState.session && (mapSearchState.phase==3 || (mapSearchState.phase==1 && mapSearchState.count)) && mapSearchResultsSession!=mapSearchState.session) {
        mapSearchResultsSession=mapSearchState.session;
        showPage(2); lv_obj_set_hidden(mapSearchPanel,false);
    }
    if(mapSearchState.session && mapSearchState.phase==5 && mapSearchPreviewSession!=mapSearchState.session) {
        mapSearchPreviewSession=mapSearchState.session;
        showPage(2); lv_obj_set_hidden(mapSearchPanel,false);
    }
    if(mapSearchState.session && mapSearchState.phase==4 && mapSearchCompletedSession!=mapSearchState.session) {
        mapSearchCompletedSession=mapSearchState.session;
        lv_obj_set_hidden(mapSearchPanel,true); showPage(2);
    }
    if(mapSearchState.session && mapSearchState.phase && mapSearchState.phase!=4 && mapSearchPanel && lv_obj_is_hidden(mapSearchPanel)) {
        if(currentPage!=AI_PAGE || mapSearchState.phase==5) showPage(2);
        lv_obj_set_hidden(mapSearchPanel,false); mapSearchRevision=UINT32_MAX;
    }
    refreshMapSearch();
    updateMapChrome(now);
    updateSpeedFaces(now);
    static uint32_t lastSpeedSecond=UINT32_MAX;
    if(lastSpeedSecond!=now/1000) {
        lastSpeedSecond=now/1000;
        const int speed=navStreamSpeedTenths();
        if(speed<0) lv_label_set_text(speedLabel,"--- km/h");
        else lv_label_set_text_fmt(speedLabel,"%d km/h",(speed+5)/10);
        bool changed=false;
        for(size_t i=0;i<count;++i) if(notices[i].deleting && now-notices[i].deleteAt>8000) {
            notices[i].deleting=false; changed=true;
        }
        if(changed) { feedback("Chưa xác nhận xóa"); refreshInbox(); }
    }
    if (popupUntil && static_cast<int32_t>(now - popupUntil) >= 0) hidePopup();
    if (musicFeedbackUntil && static_cast<int32_t>(now - musicFeedbackUntil) >= 0) {
        musicFeedbackUntil = 0;
        lv_label_set_text(musicStatus, musicAvailable ? (musicPlaying ? "Đang phát" : "Đã tạm dừng") :
                          "Mở nhạc trên iPhone");
    }
    if (feedbackUntil && static_cast<int32_t>(millis() - feedbackUntil) >= 0) {
        feedbackUntil = 0;
        updateInboxCount();
    }
    if (!clockValid) return;
    const int64_t utc = time(nullptr);
    if (utc == renderedSecond) return;
    renderedSecond = utc;
    const time_t local = utc + utcOffset;
    tm date{};
    gmtime_r(&local, &date);
    const float angles[] = {(date.tm_hour % 12 + date.tm_min / 60.0f) * 3.14159265f / 6,
        (date.tm_min + date.tm_sec / 60.0f) * 3.14159265f / 30, date.tm_sec * 3.14159265f / 30};
    const int lengths[] = {35, 52, 58};
    for (int i = 0; i < 3; ++i) {
        handPoints[i][0] = {72, 72};
        handPoints[i][1] = {72 + int(lengths[i] * sinf(angles[i])), 72 - int(lengths[i] * cosf(angles[i]))};
        lv_line_set_points(hands[i], handPoints[i], 2);
    }
    lv_label_set_text_fmt(clockLabel, "%02d:%02d", date.tm_hour, date.tm_min);
    lv_label_set_text_fmt(smallClock, "%02d:%02d",date.tm_hour,date.tm_min);
    lv_label_set_text_fmt(speedDigitalClock, "%02d:%02d", date.tm_hour, date.tm_min);
    lv_label_set_text_fmt(speedDialClock, "%02d:%02d", date.tm_hour, date.tm_min);
    const char* days[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};
    char text[96];
    snprintf(text, sizeof(text), "%s, %02d/%02d/%04d", days[date.tm_wday],
             date.tm_mday, date.tm_mon + 1, date.tm_year + 1900);
    lv_label_set_text(dateLabel, text);
    lv_label_set_text(speedDigitalDate, text);
    lv_label_set_text_fmt(extraLabel, "%02d", hudSettingsGet().face == 4 ? date.tm_mday : date.tm_sec);
    static int lunarDateKey=-1;
    const time_t vietnamTime=utc+7*3600; tm vietnamDate{}; gmtime_r(&vietnamTime,&vietnamDate);
    const int dayKey=(vietnamDate.tm_year+1900)*10000+(vietnamDate.tm_mon+1)*100+vietnamDate.tm_mday;
    if(lunarDateKey!=dayKey) {
        lunarDateKey=dayKey;
        const auto value=lunar::vietnamese(vietnamDate.tm_mday,vietnamDate.tm_mon+1,vietnamDate.tm_year+1900);
        if(value.day) lv_label_set_text_fmt(lunarLabel,"Âm lịch: %02d/%02d%s",value.day,value.month,value.leap ? " nhuận" : "");
        else lv_label_set_text(lunarLabel,"Âm lịch: --/--");
    }
}

void companionUiBluetoothFailed() {
    status(0xFF5A79);
    lv_label_set_text(dateLabel, "Không khởi tạo được Bluetooth");
}

bool companionUiNavigationActive() { return mapLive && currentPage == 2; }

void companionUiPrintStatus() {
    Serial.printf("Destination UI: phase=%u; panel=%u; start=%u; page=%d\n",unsigned(mapSearchState.phase),
        mapSearchPanel && lv_obj_is_visible(mapSearchPanel),mapSearchStart && lv_obj_is_visible(mapSearchStart),currentPage);
    lv_mem_monitor_t memory{}; lv_mem_monitor(&memory);
    Serial.printf("LVGL memory: total=%u; free=%u; largest=%u; used_pct=%u; integrity=%u\n",
        unsigned(memory.total_size),unsigned(memory.free_size),unsigned(memory.free_biggest_size),
        unsigned(memory.used_pct),unsigned(lv_mem_test()));
    lv_point_t clockSize, dateSize;
    lv_text_get_size(&clockSize, "88:88", &hud_font_clock_80, 0, 0, 1000, LV_TEXT_FLAG_NONE);
    lv_text_get_size(&dateSize, "Chủ nhật, 31/10/2026", &hud_font_vietnamese_24, 0, 0, 1000, LV_TEXT_FLAG_NONE);
    Serial.printf("Home UI: glyphs=%s; clock=%dx%d; date=%dx%d; volume=%d%%\n",
                  homeFontsReady ? "OK" : "FAILED", int(clockSize.x), int(clockSize.y),
                  int(dateSize.x), int(dateSize.y), musicVolumePercent);
    Serial.printf("Maps UI: notification=%u; text=24px; source=ANCS\n", hasNavigationNotice);
    Serial.printf("HUD layout: content=300x200; tabs=0; page=%d; swipes=%u\n", currentPage, swipeCount);
    Serial.printf("AI chat: turns=%u; holding=%u; drawer=%u\n",unsigned(aiTurnCount),aiHolding,currentPage==AI_PAGE);
    Serial.printf("HUD colors: background=%06X; text=%06X; accent=%06X\n", BG, WHITE, CYAN);
    Serial.printf("HUD notices: count=%u; detail=%u; hidden=%u; GPS_speed_tenths=%d\n",unsigned(count),detailOpen,unsigned(hiddenCount),navStreamSpeedTenths());
    const bool smallFontOk=fontRendersGlyph('A',&hud_font_vietnamese_14) &&
        fontRendersGlyph(0x1EAF,&hud_font_vietnamese_14);
    Serial.printf("Inbox rows: lines=3; app=14px; title=18px; content=18px; preview=single-line; font=%s\n",
        smallFontOk ? "OK" : "FAILED");
    unsigned advertised=0,labelled=0,eligible=0,dated=0;
    for(size_t i=0;i<count;++i) {
        advertised+=notices[i].negativeActionAvailable; labelled+=notices[i].negativeAction[0]!=0;
        eligible+=notices[i].canDismiss; dated+=strlen(notices[i].date)==15;
    }
    Serial.printf("Notice capabilities: negative=%u; label=%u; dismiss=%u; dated=%u; hidden_fingerprints=%u\n",
        advertised,labelled,eligible,dated,unsigned(hiddenFingerprintCount));
}
