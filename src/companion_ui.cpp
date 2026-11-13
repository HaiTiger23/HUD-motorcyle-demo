#include "companion_ui.h"
#include "battery.h"
#include "nav_stream.h"
#include "hud_settings.h"
#include "ai_voice.h"
#include "wallpaper.h"
#include <nvs.h>
#include <esp_idf_version.h>
#include <NimBLEDevice.h>
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
#include <src/libs/tjpgd/tjpgd.h>
#include <atomic>
#include <ai_gesture.h>
#include <hud_destination_ui.h>
#include <ai_typewriter.h>

uint8_t autoBrightnessLevel();
void buildWallFace();
void buildInfograph();
void formatDistance(char* out, size_t size, uint32_t metres);
void setIfChanged(lv_obj_t* obj, const char* text);
// Style setters that skip unchanged values: every LVGL style write invalidates the
// object, and over a photo each redraw is expensive.
inline void textColorIfChanged(lv_obj_t* obj, uint32_t rgb) {
    if ((lv_color_to_u32(lv_obj_get_style_text_color(obj, LV_PART_MAIN)) & 0xFFFFFF) != (rgb & 0xFFFFFF))
        lv_obj_set_style_text_color(obj, lv_color_hex(rgb), 0);
}
inline void bgColorIfChanged(lv_obj_t* obj, uint32_t rgb) {
    if ((lv_color_to_u32(lv_obj_get_style_bg_color(obj, LV_PART_MAIN)) & 0xFFFFFF) != (rgb & 0xFFFFFF))
        lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), 0);
}
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
lv_obj_t* batteryLabel;
bool wakeRequested=false;
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
// Riding face: replaces the chosen face on page 0 while a trip is active.
lv_obj_t *rideFace,*rideSpeed,*rideUnit,*rideClock,*rideDate,*rideWeather,*rideLeftTitle,*rideLeft,*rideCard,*rideArrow,*rideArrowIcon,*rideTurn,*rideStreet;
lv_obj_t* rideThen;
bool rideVisible=false;   // ride face on screen (trip, or chosen as the default face)
bool rideTrip=false;      // a trip is actually running
constexpr uint8_t RIDE_FACE = 6;
char lunarText[32] = "Âm --/--";
// Photo background (any face) and the full-screen "iPhone" lock-screen face (8).
constexpr uint8_t PHONE_FACE = 8;
// "Infograph" face (9): Apple Watch Infograph Modular style, full screen.
constexpr uint8_t INFO_FACE = 9;
constexpr uint32_t KM_GOAL = 30;  // ring target for today's distance
lv_obj_t *igFace,*igTime,*igDay,*igDate,*igCard,*igCardTitle,*igCardValue,*igCardSub,*igRings[4],*igRingValues[4];
lv_obj_t *wallFace,*wallImage,*wallDim,*wallTopShade,*wallBottomShade,*wallDate,*wallClock,*wallHint,*wallPills[3],*wallPillValues[3];
int wallState=-1;   // cached layout: -1 unknown, 0 off, 1 photo behind a face, 2 lock-screen layout
lv_image_dsc_t wallDsc{};
bool wallVisible=false;
uint32_t wallShownRevision=0;
bool rainWarned=false;
float speedDialDisplayed = -1;
uint32_t speedDialUpdatedAt = 0;
uint32_t appliedRevision = 0;
lv_obj_t* noticePopup;
lv_obj_t* popupApp;
lv_obj_t* popupText;
lv_obj_t* popupBadge;   // coloured circle with the app's initial
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
// Full-screen navigation overlay: turn card on top, info bar at the bottom.
lv_obj_t *navCard,*navArrow,*navArrowIcon,*navDistance,*navStreet,*navBar,*navEta,*navSpeed,*navSpeedRing,*navStatus;
lv_obj_t *qsStatus=nullptr,*infoStatus=nullptr;   // "21:53 · 86%" on full-screen panels
lv_obj_t* navThen;
bool mapFull=false;
lv_obj_t* mapDoneButton=nullptr;   // small "Xong" chip shown instead of the dock in hand-control mode
lv_obj_t *mapTouchSurface=nullptr, *mapZoomIn=nullptr, *mapZoomOut=nullptr, *mapStaleLabel=nullptr;
lv_obj_t *mapVehicleDot=nullptr, *mapVehicleArrow=nullptr;
constexpr uint32_t MAP_ARROW_ON_DARK=0xFF9E28, MAP_ARROW_ON_LIGHT=0xB94D00;
hudnav::MapMotion mapVisibleMotion;
int mapScrollOffset=0, mapScrollCorrection=0;
uint32_t mapScrollCorrectionAt=0, mapLastShiftAt=0;
bool mapScrollAtLimit=false;
uint32_t mapMotionShifts=0, mapMotionFrames=0, mapMotionAligned=0, mapMotionLimits=0;
// Local preview while the phone prepares the next frame: drag offset and zoom.
bool mapPreviewActive=false;
uint32_t mapPreviewAt=0;
uint32_t mapDragFrame=0;   // frame the accumulated drag is measured from (0 = none)
uint32_t mapMicPressedAt=0, mapMicStartedAt=0;
bool mapMicLatched=false;   // tap mode: keeps listening after the finger lifts
// Two-finger pinch/rotate: classified in the touch callback, applied on the UI tick.
struct MapPinch { volatile bool active=false, ended=false; float startDist=0, startAngle=0, scale=1, angle=0; } mapPinch;
// Hold anywhere on the map for 3 s to end the trip.
lv_obj_t *mapHoldRing=nullptr,*mapHoldArc=nullptr;
uint32_t mapHoldStart=0; lv_point_t mapHoldPoint{}; bool mapHoldArmed=false, mapHoldFired=false;
void resetMapPreview() {
    if(!mapPreviewActive || !mapImage) return;
    mapPreviewActive=false;
    lv_obj_set_pos(mapImage,-mapVisibleMotion.marginX,-mapVisibleMotion.marginY);
    lv_image_set_scale(mapImage,LV_SCALE_NONE);
    lv_image_set_rotation(mapImage,0);
    lv_image_set_pivot(mapImage,160+mapVisibleMotion.marginX,120+mapVisibleMotion.marginY);
}
lv_obj_t* googleHeading;
lv_obj_t* googleText;
phone::Notice googleGuide;
bool hasGoogleGuide = false;
uint32_t googleGuideAt = 0;
lv_image_dsc_t mapFrame{};
uint32_t mapReceivedAt = 0;
bool mapLive = false;
lv_obj_t* pairingPanel;
// Owner-PIN gate shown before the pairing code when a PIN is set.
lv_obj_t *pinPanel,*pinDots,*pinHint;
char pinTyped[5]={};
uint32_t pendingPasskey=0;
bool pendingPasskeyValid=false;
uint8_t pinFailures=0;
uint32_t pinLockedUntil=0, pairUnlockedUntil=0;
// Lock: without the owner's iPhone only the clock page is reachable.
lv_obj_t *lockSheet,*lockPinButton;
bool pinUnlocked=false, pinForUnlock=false;
volatile bool pendingLockTap=false;
uint32_t lockSheetUntil=0;
lv_obj_t* pairingCode;
lv_obj_t* musicTitle;
lv_obj_t* musicArtist;
lv_obj_t* musicStatus;
lv_obj_t* musicVolume;
lv_obj_t* musicVolumeBar;
lv_obj_t* musicPlayer;
lv_obj_t* rowBadges[10];
lv_obj_t* rowBadgeText[10];
// Quick settings (swipe down from the top edge).
lv_obj_t* qsPanel=nullptr;
// Device information screen, opened from quick settings.
lv_obj_t *infoPanel=nullptr,*infoList=nullptr;
constexpr int INFO_ROWS=20;
lv_obj_t *infoValues[INFO_ROWS];
uint32_t infoRefreshedAt=0;
lv_obj_t *qsBrightValue,*qsVolumeValue,*qsTiles[3],*qsTileValues[3];
volatile int pendingQs=0;            // set by the touch callback, applied in the tick
bool quietNotices=false;            // "Im lặng": keep in inbox, no popup, no wake
bool linkSecured=false;
bool hudLocked() { return !linkSecured && !pinUnlocked; }
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

// Map JPEGs are decoded on a background task into RGB565 so the LVGL/touch loop
// never stalls on a ~0.5 s software decode; LVGL then only blits ready pixels.
constexpr size_t MAP_W=320, MAP_H=240;   // full-screen map frames from the phone
enum MapDecodeState : int { MAP_IDLE=0, MAP_DECODING, MAP_DONE, MAP_FAILED };
uint16_t* mapPixels[2]={nullptr,nullptr};
int mapPixelTarget=0, mapPixelShown=0;
TaskHandle_t mapDecodeTask=nullptr;
std::atomic<int> mapDecodeState{MAP_IDLE};
const uint8_t* mapDecodeJpeg=nullptr;
size_t mapDecodeSize=0;
uint32_t mapDecodeMs=0;
uint32_t mapDecodeFrame=0;
hudnav::MapMotion mapDecodeMotion;
struct MapJpegSource { const uint8_t* data; size_t left; uint16_t* out; unsigned width, height; };
size_t mapJpegInput(JDEC* jd,uint8_t* buffer,size_t size) {
    auto* source=static_cast<MapJpegSource*>(jd->device);
    if(size>source->left) size=source->left;
    if(buffer) memcpy(buffer,source->data,size);
    source->data+=size; source->left-=size;
    return size;
}
// tjpgd delivers 8-bit B,G,R triplets per MCU rectangle; pack to native RGB565.
int mapJpegOutput(JDEC* jd,void* bitmap,JRECT* rect) {
    auto* source=static_cast<MapJpegSource*>(jd->device);
    if(rect->right>=source->width || rect->bottom>=source->height) return 0;
    const uint8_t* pixel=static_cast<const uint8_t*>(bitmap);
    for(unsigned y=rect->top;y<=rect->bottom;++y) {
        uint16_t* row=source->out+y*source->width;
        for(unsigned x=rect->left;x<=rect->right;++x,pixel+=3)
            row[x]=uint16_t(((pixel[2]&0xF8)<<8)|((pixel[1]&0xFC)<<3)|(pixel[0]>>3));
    }
    return 1;
}
bool decodeMapJpeg(const uint8_t* data,size_t size,uint16_t* out,unsigned width=MAP_W,unsigned height=MAP_H) {
    static uint32_t pool[1024]; // 4 KiB tjpgd workspace; only the map task uses it.
    MapJpegSource source{data,size,out,width,height};
    JDEC decoder;
    if(jd_prepare(&decoder,mapJpegInput,pool,sizeof(pool),&source)!=JDR_OK) return false;
    if(decoder.width!=width || decoder.height!=height) return false;
    return jd_decomp(&decoder,mapJpegOutput,0)==JDR_OK;
}
void mapDecodeWorker(void*) {
    for(;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        const uint32_t started=millis();
        const bool ok=decodeMapJpeg(mapDecodeJpeg,mapDecodeSize,mapPixels[mapPixelTarget],
                                    mapDecodeMotion.width(),mapDecodeMotion.height());
        mapDecodeMs=millis()-started;
        mapDecodeState.store(ok ? MAP_DONE : MAP_FAILED,std::memory_order_release);
    }
}
void startMapDecode(const uint8_t* jpeg,size_t size) {
    if(!mapDecodeTask) return;
    mapPixelTarget=mapPixelShown^1;
    mapDecodeJpeg=jpeg; mapDecodeSize=size;
    mapDecodeState.store(MAP_DECODING,std::memory_order_release);
    xTaskNotifyGive(mapDecodeTask);
}
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
    // Over a photo, light themes switch to their dark twin so text stays readable.
    // Only a photo background forces the dark twin of a light theme.
    const auto& next = palettes[wallpaperBackground() ? config.theme % 4 : config.theme];
    BG=next.bg; PANEL=next.panel; CYAN=next.accent; WHITE=next.text;
    MUTED=next.muted; BUTTON=next.button; OVERLAY=next.overlay;
    recolor(root, old); recolor(pairingPanel, old);   // the notification pill keeps its own colours
    if(qsPanel) recolor(qsPanel, old);
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
    // Faces 6/7 used to be standalone speed faces; 6 is now the ride face and 7 is retired.
    const bool speedDigitalFace = false;
    const bool speedDialFace = false;
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
    if (config.face == RIDE_FACE || config.face == PHONE_FACE || config.face == INFO_FACE) {
        for (auto obj : {analog, clockLabel, dateLabel, extraLabel, lunarLabel}) lv_obj_set_hidden(obj, true);
    }
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
        lv_obj_set_hidden(pageDots[i],true);
        lv_obj_set_style_bg_color(pageDots[i], lv_color_hex(i == currentPage ? CYAN : MUTED), 0);
    }
    const char* names[] = {"ĐỒNG HỒ", "THÔNG BÁO", "BẢN ĐỒ", "NHẠC", "TRỢ LÝ AI"};
    lv_label_set_text(pageHeading, currentPage==2 && mapPanMode ? "KÉO BẢN ĐỒ" : currentPage==0 && hudLocked() ? "ĐÃ KHÓA" : currentPage==0 && rideVisible ? "ĐI XE" : names[currentPage]);
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

// iPhone text often carries emoji (🎁, ⚠️) that the bundled fonts lack and LVGL
// draws as a box. Keep only code points the label's font can render; optionally
// flatten line breaks for one-line rows. Spaces left by removed emoji collapse.
void setFontSafeText(lv_obj_t* target, const char* text, bool flatten) {
    const lv_font_t* font=lv_obj_get_style_text_font(target,LV_PART_MAIN);
    char out[1024]; size_t used=0;
    const auto* at=reinterpret_cast<const uint8_t*>(text ? text : "");
    while(*at && used+5<sizeof(out)) {
        uint32_t cp=*at; size_t n=1;
        if(cp>=0xF0 && at[1] && at[2] && at[3]) { cp=((cp&7)<<18)|((at[1]&63)<<12)|((at[2]&63)<<6)|(at[3]&63); n=4; }
        else if(cp>=0xE0 && at[1] && at[2]) { cp=((cp&15)<<12)|((at[1]&63)<<6)|(at[2]&63); n=3; }
        else if(cp>=0xC0 && at[1]) { cp=((cp&31)<<6)|(at[1]&63); n=2; }
        const uint8_t* bytes=at; at+=n;
        if(cp=='\r' || cp=='\t' || (flatten && cp=='\n')) cp=' ';
        if(cp==' ') { if(used && out[used-1]!=' ' && out[used-1]!='\n') out[used++]=' '; continue; }
        if(cp=='\n') { while(used && out[used-1]==' ') --used; out[used++]='\n'; continue; }
        lv_font_glyph_dsc_t glyph;
        if(cp<0x20 || !lv_font_get_glyph_dsc(font,&glyph,cp,0)) continue;
        if(n==1) out[used++]=char(cp); else { memcpy(out+used,bytes,n); used+=n; }
    }
    while(used && (out[used-1]==' ' || out[used-1]=='\n')) --used;
    out[used]=0;
    if(strcmp(lv_label_get_text(target),out)) lv_label_set_text(target,out);
}
void setInboxPreview(lv_obj_t* target, const char* text) { setFontSafeText(target,text,true); }

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
        const char* app=appName(entry.app);
        // Badge: first letter of the app name on a colour derived from the app id.
        static const uint32_t badgeColors[]={0x3FA2F5,0xB78CFF,0x45D483,0xF5B83D,0xFF7A90,0x41DAF5};
        uint32_t hash=5381; for(const char* p=entry.app;*p;++p) hash=hash*33+uint8_t(*p);
        lv_obj_set_style_bg_color(rowBadges[i],lv_color_hex(badgeColors[hash%6]),0);
        char initial[5]={}; size_t n=1; if(uint8_t(app[0])>=0xC0) { n=2; while(n<4 && (uint8_t(app[n])&0xC0)==0x80) ++n; }
        memcpy(initial,app,std::min(n,strlen(app))); lv_label_set_text(rowBadgeText[i],initial);
        char stamp[32]; if (!sourceTime(entry,stamp,sizeof(stamp),false)) receivedTime(entry,stamp,sizeof(stamp),false);
        lv_label_set_text(rowTimes[i],stamp);
        setInboxPreview(rowTitles[i],entry.deleting ? "Đang xóa trên iPhone..." : entry.title[0] ? entry.title : app);
        char preview[200]; snprintf(preview,sizeof(preview),"%s · %s",app,entry.message[0] ? entry.message : "Không có nội dung xem trước");
        setInboxPreview(rowContents[i],preview);
    }
    if (detailOpen) {
        const auto& entry=notices[selected];
        lv_label_set_text(inboxApp,appName(entry.app));
        setFontSafeText(inboxTitle,entry.title,false);
        setFontSafeText(inboxBody,entry.message[0] ? entry.message : "iPhone không chia sẻ nội dung xem trước.",false);
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
    navStreamMarkDisplayed(0);
    resetMapPreview();
    mapVisibleMotion = {};
    mapDragFrame = 0;
    if (mapImage) lv_obj_set_hidden(mapImage, true);
    if (mapVehicleDot) lv_obj_set_hidden(mapVehicleDot, true);
    if (mapVehicleArrow) lv_obj_set_hidden(mapVehicleArrow, true);
    if (mapControls) lv_obj_set_hidden(mapControls, true);
    if(mapPortraitHint) lv_obj_set_hidden(mapPortraitHint,true);
    if(mapTitle) lv_obj_set_hidden(lv_obj_get_parent(mapTitle),false);
    if(mapSource) lv_obj_set_hidden(mapSource,false);
    mapPanMode = mapDragging = false;
    if (mapPanButton) lv_obj_remove_state(mapPanButton, LV_STATE_CHECKED);
    if (mapTouchSurface) lv_obj_set_gesture_bubble(mapTouchSurface, true);
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
    const bool visible = false; // shown in the full-screen turn card now (updateNavOverlay)
    lv_obj_set_hidden(googleOverlay, true);
    if (!visible) return;
    const bool allowed = hasGoogleGuide && hudSettingsAllowNotification(googleGuide.app);
    if (!allowed) {
        lv_label_set_text(googleHeading, "Google Maps");
        lv_label_set_text(googleText, "Chờ thông báo chỉ đường...");
    } else if (millis() - googleGuideAt > 120000) {
        lv_label_set_text(googleHeading, "Google Maps");
        lv_label_set_text(googleText, "Chỉ dẫn đã cũ. Chờ cập nhật...");
    } else {
        setFontSafeText(googleHeading, googleGuide.title[0] ? googleGuide.title : "Google Maps", true);
        setFontSafeText(googleText, googleGuide.message[0] ? googleGuide.message : "Không có nội dung xem trước.", true);
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
    setFontSafeText(mapTitle, notice.title[0] ? notice.title : appName(notice.app), false);
    setFontSafeText(mapBody, notice.message[0] ? notice.message :
                      "iPhone chưa chia sẻ nội dung xem trước.", false);
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
    if (!popupUid) { hidePopup(); return; } // weather warning, not an inbox item
    detailUid=popupUid; detailOpen=true;
    refreshInbox();
    showPage(1);
}

void onPopupClose(lv_event_t*) { hidePopup(); }

// Badge colour from the app id and the first letter of its display name (same as the inbox rows).
void setPopupBadge(const char* appId, const char* name, uint32_t fixedColor = 0) {
    static const uint32_t badgeColors[]={0x3FA2F5,0xB78CFF,0x45D483,0xF5B83D,0xFF7A90,0x41DAF5};
    uint32_t hash=5381; for(const char* p=appId;*p;++p) hash=hash*33+uint8_t(*p);
    lv_obj_set_style_bg_color(popupBadge, lv_color_hex(fixedColor ? fixedColor : badgeColors[hash%6]), 0);
    char initial[5]={}; size_t n=1; if(uint8_t(name[0])>=0xC0) { n=2; while(n<4 && (uint8_t(name[n])&0xC0)==0x80) ++n; }
    memcpy(initial,name,std::min(n,strlen(name)));
    lv_label_set_text(popupApp, initial);
}

void revealPopup(uint32_t ms) {
    popupUntil = millis() + ms;
    lv_obj_set_hidden(noticePopup, false);
    lv_obj_move_foreground(noticePopup);
    lv_obj_fade_in(noticePopup, 180, 0);
}

void showNoticePopup(const phone::Notice& notice) {
    if (quietNotices) return;
    if (notice.preExisting || currentPage == 1 || lv_obj_is_visible(pairingPanel)) return;
    if (currentPage == 2 && isNavigationApp(notice.app)) return;
    popupUid = notice.uid;
    const char* app = appName(notice.app);
    setPopupBadge(notice.app, app);
    // One line: "Sender: message" (or the app name when there is no title).
    char popup[700];
    snprintf(popup, sizeof(popup), "%s: %s", notice.title[0] ? notice.title : app,
             notice.message[0] ? notice.message : "Thông báo mới");
    setFontSafeText(popupText, popup, true);
    revealPopup(6000);
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
    lv_label_set_text(musicVolume, "--%");
    lv_bar_set_value(musicVolumeBar, 0, LV_ANIM_OFF);
    lv_label_set_text(musicPlayer, "Nhạc iPhone");
    updateMusicControls();
}

void onGesture(lv_event_t* event) {
    if(hudLocked()) return;
    if(infoPanel && lv_obj_is_visible(infoPanel)) return;
    if(currentPage==AI_PAGE || aiGesture.consumes()) return;
    if(qsPanel && !lv_obj_is_hidden(qsPanel)) return;
    if (mapPanMode && lv_event_get_target(event)==mapTouchSurface) return;
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
    lv_obj_set_gesture_bubble(mapTouchSurface,!enabled);
    if (!enabled) { resetMapPreview(); mapDragFrame=0; }
    else lv_obj_set_pos(mapImage,-mapVisibleMotion.marginX,-mapVisibleMotion.marginY);
    if(enabled) lv_obj_add_state(mapPanButton,LV_STATE_CHECKED);
    else lv_obj_remove_state(mapPanButton,LV_STATE_CHECKED);
}
void onMapControl(lv_event_t* event) {
    if(!mapLive) return;
    mapMenuAt=millis();
    const int action=reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
    if(action==5) {
        const bool enabled=!mapPanMode;
        if(navStreamControl(enabled ? 5 : 3)) setMapPan(enabled);
    } else if(navStreamControl(action)) {
        if(action==3) setMapPan(false);
    }
}
void onMapMenu(lv_event_t*) { mapMenuOpen=!mapMenuOpen; mapMenuAt=millis(); }
void openMapSearch(lv_event_t*) {
    mapMenuOpen=false;
    mapSearchRevision=UINT32_MAX; lv_label_set_text(mapSearchStatus,"Giữ mic rồi nói nơi muốn đến");
    lv_obj_set_hidden(mapSearchPanel,false);
}

// Turn icons drawn as road shapes, matching the Apple Maps set. Each sits in a 44x44 tile:
// a bold route line (shaft + arrow head) and an optional dim "other road" line.
// Codes (same byte as the phone's Maneuver enum):
//  0 straight  1 left  2 right  3 slight left  4 slight right  5 U-turn  6 arrive (flag)
//  7 sharp left  8 sharp right  9 keep left  10 keep right  11 merge  12 exit/ramp left
//  13 exit/ramp right  14 roundabout straight  15 roundabout left  16 roundabout right  17 ferry
// The point arrays must stay static: LVGL keeps the pointers.
constexpr int TURN_KINDS=18;
struct TurnIcon { lv_obj_t *shaft=nullptr,*head=nullptr,*extra=nullptr,*glyph=nullptr; };
TurnIcon navTurn, rideTurnIcon;
const lv_point_precise_t TURN_SHAFT[TURN_KINDS][13]={
    {{22,37},{22,11}},
    {{28,37},{28,22},{12,22}},
    {{16,37},{16,22},{32,22}},
    {{26,37},{26,25},{15,14}},
    {{18,37},{18,25},{29,14}},
    {{28,37},{28,17},{25,11},{19,11},{16,17},{16,31}},
    {{15,37},{15,8},{31,14},{15,20}},
    {{28,37},{28,13},{14,27}},
    {{16,37},{16,13},{30,27}},
    {{22,37},{22,24},{13,12}},
    {{22,37},{22,24},{31,12}},
    {{13,37},{13,29},{22,20},{22,9}},
    {{26,37},{26,27},{14,15}},
    {{18,37},{18,27},{30,15}},
    {{22,38},{22,30},{26,29},{29,26},{30,22},{29,18},{26,15},{22,14},{22,4}},
    {{22,38},{22,30},{26,29},{29,26},{30,22},{29,18},{26,15},{22,14},{18,15},{15,18},{14,22},{4,22}},
    {{22,38},{22,30},{26,29},{29,26},{30,22},{40,22}},
    {{7,25},{37,25},{32,33},{12,33},{7,25}}};
const uint8_t TURN_SHAFT_N[TURN_KINDS]={2,3,3,3,3,6,4,3,3,3,3,4,3,3,9,12,6,5};
const lv_point_precise_t TURN_HEAD[TURN_KINDS][3]={
    {{14,19},{22,11},{30,19}},
    {{19,15},{12,22},{19,29}},
    {{25,15},{32,22},{25,29}},
    {{15,23},{15,14},{24,14}},
    {{20,14},{29,14},{29,23}},
    {{10,25},{16,32},{22,25}},
    {},
    {{14,18},{14,27},{23,27}},
    {{30,18},{30,27},{21,27}},
    {{13,21},{13,12},{22,12}},
    {{22,12},{31,12},{31,21}},
    {{14,17},{22,9},{30,17}},
    {{14,24},{14,15},{23,15}},
    {{21,15},{30,15},{30,24}},
    {{16,10},{22,4},{28,10}},
    {{10,16},{4,22},{10,28}},
    {{34,16},{40,22},{34,28}},
    {{22,25},{22,8},{33,22}}};
const uint8_t TURN_HEAD_N[TURN_KINDS]={3,3,3,3,3,3,0,3,3,3,3,3,3,3,3,3,3,3};
const lv_point_precise_t TURN_EXTRA[TURN_KINDS][13]={
    {},{},{},{},{},{},{},{},{},
    {{22,24},{31,12}},
    {{22,24},{13,12}},
    {{31,37},{31,29},{22,20}},
    {{26,37},{26,7}},
    {{18,37},{18,7}},
    {{22,30},{26,29},{29,26},{30,22},{29,18},{26,15},{22,14},{18,15},{15,18},{14,22},{15,26},{18,29},{22,30}},
    {{22,30},{26,29},{29,26},{30,22},{29,18},{26,15},{22,14},{18,15},{15,18},{14,22},{15,26},{18,29},{22,30}},
    {{22,30},{26,29},{29,26},{30,22},{29,18},{26,15},{22,14},{18,15},{15,18},{14,22},{15,26},{18,29},{22,30}},
    {{6,39},{12,36},{18,39},{24,36},{30,39},{36,36}}};
const uint8_t TURN_EXTRA_N[TURN_KINDS]={0,0,0,0,0,0,0,0,0,2,2,3,2,2,13,13,13,6};
TurnIcon makeTurnIcon(lv_obj_t* tile,lv_obj_t* glyph,uint32_t ink) {
    TurnIcon icon; icon.glyph=glyph;
    const int x=(lv_obj_get_style_width(tile,LV_PART_MAIN)-44)/2, y=(lv_obj_get_style_height(tile,LV_PART_MAIN)-44)/2;
    for(auto line : {&icon.extra,&icon.shaft,&icon.head}) {   // extra first: drawn underneath
        *line=lv_line_create(tile);
        lv_obj_set_pos(*line,x,y);
        lv_obj_set_style_line_width(*line,line==&icon.extra ? 3 : 5,0);
        lv_obj_set_style_line_rounded(*line,true,0);
        lv_obj_set_style_line_color(*line,lv_color_hex(ink),0);
        if(line==&icon.extra) lv_obj_set_style_line_opa(*line,110,0);
        lv_obj_remove_flag(*line,LV_OBJ_FLAG_CLICKABLE);
    }
    return icon;
}
// maneuver < 0: hide the road shape and show the font glyph (GPS / Google).
void setTurnIcon(TurnIcon& icon,int maneuver) {
    const bool shape=maneuver>=0 && maneuver<TURN_KINDS;
    lv_obj_set_hidden(icon.glyph,shape);
    lv_obj_set_hidden(icon.shaft,!shape);
    lv_obj_set_hidden(icon.head,!shape || !TURN_HEAD_N[shape ? maneuver : 0]);
    lv_obj_set_hidden(icon.extra,!shape || !TURN_EXTRA_N[shape ? maneuver : 0]);
    if(!shape) return;
    lv_line_set_points(icon.shaft,TURN_SHAFT[maneuver],TURN_SHAFT_N[maneuver]);
    if(TURN_HEAD_N[maneuver]) lv_line_set_points(icon.head,TURN_HEAD[maneuver],TURN_HEAD_N[maneuver]);
    if(TURN_EXTRA_N[maneuver]) lv_line_set_points(icon.extra,TURN_EXTRA[maneuver],TURN_EXTRA_N[maneuver]);
}

// "21:53 · 86%" for screens that hide the status bar.
void statusText(char* out,size_t size) {
    char clock[8]="--:--";
    if(clockValid) { const time_t local=time(nullptr)+utcOffset; tm d{}; gmtime_r(&local,&d); snprintf(clock,sizeof(clock),"%02d:%02d",d.tm_hour,d.tm_min); }
    const BatteryStatus b=batteryRead();
    if(b.present) snprintf(out,size,"%s · %d%%",clock,b.percent); else strlcpy(out,clock,size);
}

lv_obj_t* glassCard(lv_obj_t* parent,int x,int y,int w,int h) {
    auto card=box(parent,x,y,w,h,0x15191F);
    lv_obj_set_style_bg_opa(card,235,0);
    lv_obj_set_style_radius(card,14,0);
    lv_obj_set_style_border_width(card,1,0);
    lv_obj_set_style_border_color(card,lv_color_hex(0x343C46),0);
    lv_obj_remove_flag(card,LV_OBJ_FLAG_CLICKABLE);
    return card;
}

void buildNavOverlay() {
    navCard=glassCard(pages[2],6,6,308,58);
    navArrow=box(navCard,7,6,44,44,0x45D483);
    lv_obj_set_style_radius(navArrow,12,0);
    lv_obj_remove_flag(navArrow,LV_OBJ_FLAG_CLICKABLE);
    navArrowIcon=label(navArrow,0,0,44,LV_SYMBOL_GPS,0x04170C,&lv_font_montserrat_22);
    lv_obj_set_style_text_align(navArrowIcon,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(navArrowIcon);
    navTurn=makeTurnIcon(navArrow,navArrowIcon,0x04170C);
    setTurnIcon(navTurn,-1);
    navDistance=label(navCard,60,3,240,"",0xFFFFFF,&hud_font_vietnamese_24);
    lv_label_set_long_mode(navDistance,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(navDistance,28);
    navStreet=label(navCard,60,32,240,"",0xC9D1DA,&hud_font_vietnamese_14);
    lv_label_set_long_mode(navStreet,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(navStreet,20);
    navThen=label(navCard,60,51,240,"",0x8BDCF7,&hud_font_vietnamese_14);
    lv_label_set_long_mode(navThen,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(navThen,18);
    lv_obj_set_hidden(navThen,true);
    navBar=glassCard(pages[2],6,202,308,34);
    navEta=label(navBar,10,7,206,"",0xFFFFFF,&hud_font_vietnamese_14);
    lv_label_set_long_mode(navEta,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(navEta,20);
    // The full-screen map covers the status bar: clock and battery live at the bar's right end.
    navStatus=label(navBar,214,7,86,"",0xC9D1DA,&hud_font_vietnamese_14);
    lv_obj_set_style_text_align(navStatus,LV_TEXT_ALIGN_RIGHT,0);
    // Speed: a round badge in the bottom-right corner, above the bar.
    navSpeedRing=glassCard(pages[2],240,122,74,74);
    lv_obj_set_style_radius(navSpeedRing,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_border_width(navSpeedRing,2,0);
    navSpeed=label(navSpeedRing,0,10,74,"--",0xFFFFFF,&lv_font_montserrat_34);
    lv_obj_set_style_text_align(navSpeed,LV_TEXT_ALIGN_CENTER,0);
    auto unit=label(navSpeedRing,0,48,74,"km/h",0x98A2AD,&lv_font_montserrat_10);
    lv_obj_set_style_text_align(unit,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_hidden(navSpeedRing,true);
    lv_obj_set_hidden(navCard,true); lv_obj_set_hidden(navBar,true);
    mapHoldRing=glassCard(pages[2],95,55,130,130);
    lv_obj_set_style_radius(mapHoldRing,LV_RADIUS_CIRCLE,0);
    mapHoldArc=lv_arc_create(mapHoldRing);
    lv_obj_set_size(mapHoldArc,112,112); lv_obj_center(mapHoldArc);
    lv_arc_set_bg_angles(mapHoldArc,0,360); lv_arc_set_rotation(mapHoldArc,270);
    lv_arc_set_range(mapHoldArc,0,100); lv_arc_set_value(mapHoldArc,0);
    lv_obj_remove_style(mapHoldArc,nullptr,LV_PART_KNOB); lv_obj_remove_flag(mapHoldArc,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(mapHoldArc,8,LV_PART_MAIN); lv_obj_set_style_arc_width(mapHoldArc,8,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(mapHoldArc,lv_color_hex(0x3A424D),LV_PART_MAIN);
    lv_obj_set_style_arc_color(mapHoldArc,lv_color_hex(0xFF453A),LV_PART_INDICATOR);
    auto holdText=label(mapHoldRing,0,44,130,"Giữ để\nkết thúc",0xFFFFFF,&hud_font_vietnamese_14);
    lv_obj_set_style_text_align(holdText,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_hidden(mapHoldRing,true);
}

void endTripFromHold() {
    if(navStreamControl(6)) { mapStopPending=true; mapStopAt=millis(); mapMenuOpen=false; }
    else { lv_label_set_text(mapStopMessage,"Chưa gửi được lệnh. Kiểm tra kết nối rồi thử lại."); lv_obj_set_hidden(mapStopDialog,false); }
}

// Full screen while a live map is shown: the map page covers the status bar.
void setMapFullscreen(bool on) {
    if(on==mapFull) return;
    mapFull=on;
    lv_obj_set_pos(pages[2],on ? 0 : 10,on ? 0 : 32);
    lv_obj_set_size(pages[2],on ? 320 : 300,on ? 240 : 200);
    lv_obj_set_style_radius(pages[2],on ? 0 : 10,0);
    if(on) lv_obj_move_foreground(pages[2]);
    // Modal panels keep their 300x200 layout; centre them on the full screen.
    lv_obj_set_pos(mapSearchPanel,on ? 10 : 0,on ? 20 : 0);
    lv_obj_set_pos(mapStopDialog,on ? 18 : 8,on ? 50 : 30);
}

void showFollowingTurn(lv_obj_t* caption,const TripInfo& trip,bool visible) {
    static const char* const directions[] = {"", "rẽ trái", "rẽ phải", "chếch trái", "chếch phải", "quay đầu", "",
        "rẽ gắt trái", "rẽ gắt phải", "giữ trái", "giữ phải", "nhập làn", "ra nhánh trái", "ra nhánh phải",
        "vào vòng xuyến", "vòng xuyến trái", "vòng xuyến phải"};
    const bool show = visible && trip.followingManeuver < sizeof(directions)/sizeof(directions[0]);
    lv_obj_set_hidden(caption,!show);
    if (show) {
        char text[80];
        snprintf(text,sizeof(text),"Rồi %s · %u m",directions[trip.followingManeuver],unsigned(trip.followingMetres));
        setFontSafeText(caption,text,true);
    }
}

void updateNavOverlay(uint32_t now) {
    static uint32_t last=0;
    if(now-last<250) return;
    last=now;
    const bool modal=lv_obj_is_visible(mapSearchPanel) || (mapStopDialog && lv_obj_is_visible(mapStopDialog));
    TripInfo trip{};
    const bool tripInfo=navStreamTrip(trip);
    const bool google=navStreamExternalGuidance();
    const bool show=mapLive && !modal;
    const bool twoTurns=tripInfo && !google && trip.followingManeuver!=255;
    lv_obj_set_hidden(navCard,!show || (!tripInfo && !google));
    lv_obj_set_hidden(navBar,!show);
    const bool dock=mapTripActive && mapMenuOpen && !mapPanMode;
    lv_obj_set_hidden(navSpeedRing,!show || dock);
    if(!show) return;
    lv_obj_set_height(navCard,twoTurns ? 73 : 58);
    showFollowingTurn(navThen,trip,twoTurns);
    char text[96];
    if(google) {
        const bool allowed=hasGoogleGuide && hudSettingsAllowNotification(googleGuide.app);
        const bool stale=allowed && millis()-googleGuideAt>120000;
        setIfChanged(navDistance,!allowed ? "Google Maps" : stale ? "Chỉ dẫn đã cũ" : (googleGuide.title[0] ? googleGuide.title : "Google Maps"));
        if(allowed && !stale) setFontSafeText(navStreet,googleGuide.message[0] ? googleGuide.message : "",true);
        else setIfChanged(navStreet,"Chờ thông báo chỉ đường...");
        setIfChanged(navArrowIcon,LV_SYMBOL_GPS); setTurnIcon(navTurn,-1);
        bgColorIfChanged(navArrow,0x3FA2F5);
    } else if(tripInfo) {
        if(trip.maneuver==6 && trip.turnMetres<=30) strlcpy(text,"Đã đến nơi",sizeof(text));
        else formatDistance(text,sizeof(text),trip.turnMetres);
        setIfChanged(navDistance,text);
        setFontSafeText(navStreet,trip.instruction,true);
        static const char* icons[]={LV_SYMBOL_UP,LV_SYMBOL_LEFT,LV_SYMBOL_RIGHT,LV_SYMBOL_LEFT,LV_SYMBOL_RIGHT,LV_SYMBOL_REFRESH,LV_SYMBOL_GPS};
        (void)icons; setTurnIcon(navTurn,trip.maneuver<TURN_KINDS ? trip.maneuver : 0);
        bgColorIfChanged(navArrow,0x45D483);
    }
    // Close to the turn: the card lights up.
    const bool soon=tripInfo && !google && trip.turnMetres<=100 && trip.maneuver!=6;
    lv_obj_set_style_border_color(navCard,lv_color_hex(soon ? 0x45D483 : 0x343C46),0);
    lv_obj_set_style_border_width(navCard,soon ? 2 : 1,0);
    if(tripInfo && trip.simulating) {
        char left[24]; formatDistance(left,sizeof(left),trip.remainingMetres);
        if(trip.remainingMetres==0) strlcpy(text,"CHẠY THỬ · Đã đến nơi",sizeof(text));
        else snprintf(text,sizeof(text),"CHẠY THỬ · Còn %s",left);
    } else if(tripInfo && trip.etaMinute<1440 && clockValid) {
        const time_t local=time(nullptr)+utcOffset; tm d{}; gmtime_r(&local,&d);
        int minutes=int(trip.etaMinute)-(d.tm_hour*60+d.tm_min); if(minutes<0) minutes+=1440;
        char left[24]; formatDistance(left,sizeof(left),trip.remainingMetres);
        if(minutes>=60) snprintf(text,sizeof(text),"Đến %02u:%02u · %s · %dg%02d",unsigned(trip.etaMinute/60),unsigned(trip.etaMinute%60),left,minutes/60,minutes%60);
        else snprintf(text,sizeof(text),"Đến %02u:%02u · %s · %d phút",unsigned(trip.etaMinute/60),unsigned(trip.etaMinute%60),left,minutes);
    } else strlcpy(text,google ? "Đang dẫn đường bằng Google Maps" : "Đang dẫn đường",sizeof(text));
    setIfChanged(navEta,text);
    const int tenths=navStreamSpeedTenths();
    if(tenths>=0) snprintf(text,sizeof(text),"%d",(tenths+5)/10); else strlcpy(text,"--",sizeof(text));
    setIfChanged(navSpeed,text);
    const bool fast=tenths>600;
    textColorIfChanged(navSpeed,fast ? 0xFF5A79 : 0xFFFFFF);
    lv_obj_set_style_border_color(navSpeedRing,lv_color_hex(fast ? 0xFF5A79 : 0x343C46),0);
    statusText(text,sizeof(text));
    setIfChanged(navStatus,text);
}
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
void updateMapPinch(uint32_t now);
void updateMapMotion(uint32_t now, bool modal) {
    const bool tracking = mapLive && mapVisibleMotion.speed && !mapPanMode && !mapPreviewActive && !modal;
    lv_obj_set_hidden(mapVehicleDot,!tracking || mapVisibleMotion.arrow);
    lv_obj_set_hidden(mapVehicleArrow,!tracking || !mapVisibleMotion.arrow);
    if (!tracking || currentPage != 2) return;
    if (now-mapLastShiftAt < 30) return;
    const int offset = mapVisibleMotion.offset(now-mapVisibleMotion.startedAt,mapScrollCorrection,now-mapScrollCorrectionAt);
    if (offset != mapScrollOffset || lv_obj_get_y(mapImage) != offset-mapVisibleMotion.marginY) {
        mapScrollOffset=offset;
        lv_obj_set_y(mapImage,offset-mapVisibleMotion.marginY);
        mapLastShiftAt=now;
        ++mapMotionShifts;
    }
    if (!mapScrollAtLimit && offset==mapVisibleMotion.marginY) {
        mapScrollAtLimit=true;
        ++mapMotionLimits;
    }
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
    // Hand control needs the whole map: the dock hides and only the small "Xong" chip stays.
    const bool controls=mapTripActive && mapMenuOpen && !modal && !mapPanMode;
    const bool browsing=mapLive && mapPanMode && !modal;
    lv_obj_set_hidden(mapDoneButton,!browsing);
    lv_obj_set_hidden(mapZoomIn,!browsing);
    lv_obj_set_hidden(mapZoomOut,!browsing);
    lv_obj_set_hidden(mapTouchSurface,!mapLive);
    lv_obj_set_hidden(mapStaleLabel,!(mapLive && !modal && uint32_t(now-mapReceivedAt)>15000));
    setMapFullscreen(mapLive);
    lv_obj_set_hidden(mapControls,!controls);
    lv_obj_set_hidden(mapMenuButton,true);      // tapping the map opens the dock instead
    lv_obj_set_hidden(speedBox,true);           // speed lives in the bottom bar
    lv_obj_set_hidden(mapSearchButton,true);    // search is in the dock (trip) or the home mic
    updateNavOverlay(now);
    updateMapPinch(now);
    updateMapMotion(now,modal);
    if(mapMicLatched && uint32_t(now-mapMicStartedAt)>10000) { aiVoiceStop(); mapMicLatched=false; mapHomeRecording=false; }
    if(mapPreviewActive && !mapDragging && uint32_t(now-mapPreviewAt)>3000) { resetMapPreview(); mapDragFrame=0; }
    const bool home=!mapTripActive && !modal;
    lv_obj_set_hidden(mapHomeUi.panel,!home);
    // Trip running but no frame yet (phone fetching GPS / drawing): say so instead of a blank page.
    if(mapTripActive && !mapLive && !modal) {
        lv_obj_set_hidden(lv_obj_get_parent(mapTitle),false);
        setIfChanged(mapTitle,"Đang chờ bản đồ");
        setIfChanged(mapBody,"iPhone đang lấy GPS và vẽ bản đồ…");
    }
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
// Map mic: hold to talk (send on release) or tap once to start and tap again to send.
// A quick tap used to cancel the request before the phone was ready, so nothing was searched.
bool mapMicPress(lv_obj_t* statusLabel) {
    if(mapMicLatched) { aiVoiceStop(); mapMicLatched=false; return false; }   // second tap: send
    mapMicPressedAt=millis();
    if(!aiVoiceStart(1)) { lv_label_set_text(statusLabel,"Chưa sẵn sàng. Kiểm tra HUD Connect."); return false; }
    mapMicStartedAt=millis();
    return true;
}
void mapMicRelease(lv_obj_t* statusLabel) {
    if(millis()-mapMicPressedAt<600) {    // quick tap: keep listening until the next tap
        mapMicLatched=true;
        lv_label_set_text(statusLabel,"Đang nghe... chạm mic lần nữa để tìm");
    } else aiVoiceStop();                  // held: send now
}
void onMapHomeMic(lv_event_t* event) {
    const auto code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) {
        const bool latchedBefore=mapMicLatched;
        mapHomeRecording=mapMicPress(mapHomeUi.heading);
        if(latchedBefore) { lv_obj_set_hidden(mapSearchPanel,false); mapSearchRevision=UINT32_MAX; }
    } else if(code==LV_EVENT_RELEASED && mapHomeRecording) {
        mapMicRelease(mapHomeUi.heading);
        if(!mapMicLatched) { mapHomeRecording=false; lv_obj_set_hidden(mapSearchPanel,false); mapSearchRevision=UINT32_MAX; }
    } else if(code==LV_EVENT_PRESS_LOST && mapHomeRecording && !mapMicLatched) {
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
    static bool recording=false;
    if(code==LV_EVENT_PRESSED) {
        recording=mapMicPress(mapSearchStatus);
        if(recording) lv_label_set_text(mapSearchStatus,"Đang chuẩn bị mic…");
    } else if(code==LV_EVENT_RELEASED && recording) {
        mapMicRelease(mapSearchStatus);
        if(!mapMicLatched) recording=false;
    } else if(code==LV_EVENT_PRESS_LOST && recording && !mapMicLatched) { aiVoiceCancel(); recording=false; }
}
void onMapSearchChoose(lv_event_t* event) {
    const int index=reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
    if(aiVoiceDestinationControl(1,index)) lv_label_set_text(mapSearchStatus,"Đang tính tuyến…");
    else lv_label_set_text(mapSearchStatus,"Chưa gửi được lựa chọn. Hãy thử lại.");
}
void onMapSearchStart(lv_event_t*) {
    if(aiVoiceDestinationControl(2)) { lv_obj_set_hidden(mapSearchPanel,true); mapSearchRevision=UINT32_MAX; }
}
// Latest recognized speech for the map search panel (copied in companionUiTick).
char heardQuestion[192]="";
bool aiRecordingNow=false;
void refreshMapSearch() {
    if(!mapSearchPanel || lv_obj_is_hidden(mapSearchPanel)) return;
    aiVoiceDestinationRead(mapSearchState);
    char voiceStatus[128]; aiVoiceReadStatus(voiceStatus);
    const char* message=mapSearchState.status[0] ? mapSearchState.status : voiceStatus;
    const char* items[5]; for(int i=0;i<5;++i) items[i]=mapSearchState.items[i];
    mapSearchUi.show(mapSearchState.phase,message[0] ? message : "Giữ mic rồi nói địa điểm",
        mapSearchState.count,items,lv_obj_get_width(mapSearchPanel),lv_obj_get_height(mapSearchPanel),mapSearchState.session,
        heardQuestion);

}
void onMapTouch(lv_event_t* event) {
    if(!mapLive) return;
    auto input=lv_indev_active();
    if(!input) return;
    const auto code=lv_event_get_code(event);
    // Hold still for 3 s anywhere on the map: end the trip (ring fills after 0.5 s).
    if(code==LV_EVENT_PRESSED) {
        // Not while hand-controlling the map: a resting finger must never end the trip.
        mapHoldArmed=mapTripActive && !mapPinch.active && !mapPanMode; mapHoldFired=false;
        mapHoldStart=millis(); lv_indev_get_point(input,&mapHoldPoint);
    } else if(code==LV_EVENT_PRESSING && mapHoldArmed) {
        lv_point_t point; lv_indev_get_point(input,&point);
        const uint32_t held=millis()-mapHoldStart;
        if(abs(point.x-mapHoldPoint.x)>15 || abs(point.y-mapHoldPoint.y)>15 || mapPinch.active) {
            mapHoldArmed=false; lv_obj_set_hidden(mapHoldRing,true);
        } else if(held>=3000) {
            mapHoldArmed=false; mapHoldFired=true; lv_obj_set_hidden(mapHoldRing,true);
            endTripFromHold();
        } else if(held>500) {
            lv_arc_set_value(mapHoldArc,int((held-500)*100/2500));
            if(lv_obj_is_hidden(mapHoldRing)) { lv_obj_set_hidden(mapHoldRing,false); lv_obj_move_foreground(mapHoldRing); }
        }
    } else if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        mapHoldArmed=false; lv_obj_set_hidden(mapHoldRing,true);
    }
    // Short tap (not a hold): show/hide the dock.
    if(code==LV_EVENT_SHORT_CLICKED && !mapPanMode && !mapHoldFired) { onMapMenu(nullptr); return; }
    if(!mapPanMode) return;
    mapMenuAt=millis();
    // Drags accumulate until the phone's re-centred frame arrives: the offset is always
    // measured from the frame on screen, so repeated drags never snap back.
    static lv_point_t dragBase{};
    if(code==LV_EVENT_PRESSED) {
        lv_indev_get_point(input,&mapDragStart); mapDragging=true;
        if(!mapPreviewActive || !mapDragFrame) { dragBase={-mapVisibleMotion.marginX,-mapVisibleMotion.marginY}; mapDragFrame=navStreamDisplayedFrame(); }
        else dragBase={lv_obj_get_x(mapImage),lv_obj_get_y(mapImage)};
    }
    else if(code==LV_EVENT_PRESSING && mapDragging && !mapPinch.active) {
        // Move the frame with the finger now; the phone's re-centred frame follows.
        lv_point_t point; lv_indev_get_point(input,&point);
        lv_obj_set_pos(mapImage,dragBase.x+point.x-mapDragStart.x,dragBase.y+point.y-mapDragStart.y);
        mapPreviewActive=true; mapPreviewAt=millis();
    }
    else if(code==LV_EVENT_PRESS_LOST) { mapDragging=false; }
    else if(code==LV_EVENT_RELEASED && mapDragging) {
        mapDragging=false;
        // The map fills the screen 1:1, so a drag in pixels is a drag in map pixels.
        const int dx=constrain(lv_obj_get_x(mapImage)+mapVisibleMotion.marginX,-int(MAP_W),int(MAP_W));
        const int dy=constrain(lv_obj_get_y(mapImage)+mapVisibleMotion.marginY,-int(MAP_H),int(MAP_H));
        if(abs(dx)>=10 || abs(dy)>=10) navStreamControl(4,dx,dy,mapDragFrame);
    }
}

// Pinch/rotate preview and, when the fingers lift, one command to the phone.
void updateMapPinch(uint32_t now) {
    if(mapPinch.active) {
        const int ox=lv_obj_get_x(mapImage), oy=lv_obj_get_y(mapImage);
        lv_image_set_pivot(mapImage,MAP_W/2-ox,MAP_H/2-oy);   // around the screen centre
        lv_image_set_scale(mapImage,uint32_t(256*mapPinch.scale));
        lv_image_set_rotation(mapImage,int(mapPinch.angle*10));
        mapPreviewActive=true; mapPreviewAt=now;
        if(!mapDragFrame) mapDragFrame=navStreamDisplayedFrame();
    }
    if(!mapPinch.ended) return;
    mapPinch.ended=false;
    const int scale=int(mapPinch.scale*100+0.5f), angle=int(mapPinch.angle+(mapPinch.angle>=0 ? 0.5f : -0.5f));
    if(abs(scale-100)<5 && abs(angle)<4) { lv_image_set_scale(mapImage,LV_SCALE_NONE); lv_image_set_rotation(mapImage,0); return; }
    navStreamControl(7,scale,angle,mapDragFrame ? mapDragFrame : navStreamDisplayedFrame());
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
        if(request.action==5) { valid=request.value<10; settings.face=request.value; }
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
    lv_label_set_text(aiBodies[row],text[0] ? text : (waiting ? (aiRecordingNow ? "Đang nghe..." : "Đang trả lời...") : "Không có câu trả lời"));
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

// A pill whose icon and text are centred by flex layout (no hand-placed offsets).
lv_obj_t* pillButton(lv_obj_t* parent,int x,int y,int w,int h,const char* icon,const char* text,uint32_t fill) {
    auto button=lv_button_create(parent);
    lv_obj_set_pos(button,x,y); lv_obj_set_size(button,w,h);
    lv_obj_set_style_radius(button,h/2,0); lv_obj_set_style_shadow_width(button,0,0);
    lv_obj_set_style_bg_color(button,lv_color_hex(fill),0);
    lv_obj_set_style_pad_all(button,0,0);
    lv_obj_set_flex_flow(button,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(button,8,0);
    if(icon) {
        auto i=lv_label_create(button); lv_label_set_text(i,icon);
        lv_obj_set_style_text_font(i,&lv_font_montserrat_18,0); lv_obj_set_style_text_color(i,lv_color_hex(WHITE),0);
    }
    auto t=lv_label_create(button); lv_label_set_text(t,text);
    lv_obj_set_style_text_font(t,textFont,0); lv_obj_set_style_text_color(t,lv_color_hex(WHITE),0);
    return button;
}
// ---------------- Device information ----------------
const char* const INFO_LABELS[INFO_ROWS] = {
    "#Chip", "Mẫu chip", "CPU", "Nhiệt độ chip", "Đã chạy",
    "#Bộ nhớ", "Chương trình", "RAM trống", "PSRAM trống", "Ảnh nền (LittleFS)", "Cài đặt (NVS)",
    "#Pin", "Pin", "Điện áp",
    "#Kết nối", "iPhone", "Bluetooth MAC",
    "#Phần mềm", "Firmware", "ESP-IDF / LVGL"};

void formatBytes(char* out, size_t size, size_t bytes) {
    if (bytes >= 1024 * 1024) snprintf(out, size, "%u,%u MB", unsigned(bytes / 1048576), unsigned(bytes % 1048576 * 10 / 1048576));
    else snprintf(out, size, "%u KB", unsigned((bytes + 512) / 1024));
}

void refreshInfoPanel() {
    char a[24], b[24], v[64];
    auto set = [&](int row, const char* text) { if (infoValues[row]) setIfChanged(infoValues[row], text); };
    snprintf(v, sizeof(v), "%s rev %u", ESP.getChipModel(), unsigned(ESP.getChipRevision())); set(1, v);
    snprintf(v, sizeof(v), "%u nhân · %u MHz", unsigned(ESP.getChipCores()), unsigned(ESP.getCpuFreqMHz())); set(2, v);
    snprintf(v, sizeof(v), "%.0f °C", temperatureRead()); set(3, v);
    const uint32_t up = millis() / 1000;
    if (up >= 3600) snprintf(v, sizeof(v), "%u giờ %u phút", unsigned(up / 3600), unsigned(up / 60 % 60));
    else snprintf(v, sizeof(v), "%u phút %u giây", unsigned(up / 60), unsigned(up % 60));
    set(4, v);
    const size_t sketch = ESP.getSketchSize(), slot = sketch + ESP.getFreeSketchSpace();
    formatBytes(a, sizeof(a), sketch); formatBytes(b, sizeof(b), slot);
    snprintf(v, sizeof(v), "%s / %s (%u%%)", a, b, unsigned(slot ? sketch * 100 / slot : 0)); set(6, v);
    formatBytes(a, sizeof(a), ESP.getFreeHeap()); formatBytes(b, sizeof(b), ESP.getHeapSize());
    snprintf(v, sizeof(v), "%s / %s", a, b); set(7, v);
    formatBytes(a, sizeof(a), ESP.getFreePsram()); formatBytes(b, sizeof(b), ESP.getPsramSize());
    snprintf(v, sizeof(v), "%s / %s", a, b); set(8, v);
    size_t used = 0, total = 0;
    if (wallpaperStorage(used, total)) {
        formatBytes(a, sizeof(a), total - used); formatBytes(b, sizeof(b), total);
        snprintf(v, sizeof(v), "trống %s / %s", a, b);
    } else strlcpy(v, "chưa sẵn sàng", sizeof(v));
    set(9, v);
    nvs_stats_t nvs{};
    if (nvs_get_stats(nullptr, &nvs) == ESP_OK)
        snprintf(v, sizeof(v), "%u / %u ô trống", unsigned(nvs.free_entries), unsigned(nvs.total_entries));
    else strlcpy(v, "--", sizeof(v));
    set(10, v);
    const BatteryStatus bat = batteryRead();
    if (bat.percent >= 0) snprintf(v, sizeof(v), "%d%%%s", bat.percent,
        bat.chargingEstimated ? " · có thể đang sạc" : bat.usbHost ? " · USB" : "");
    else if (bat.present) strlcpy(v, "Đang đo pin…", sizeof(v));
    else strlcpy(v, bat.usbHost ? "USB · chưa thấy pin" : "Chưa thấy pin", sizeof(v));
    set(12, v);
    snprintf(v, sizeof(v), "%d mV", bat.millivolts); set(13, v);
    set(15, !phoneLinkEnabled() ? "BLE đang tắt" : linkSecured ? "Đã kết nối" : "Chưa kết nối");
    set(16, NimBLEDevice::getAddress().toString().c_str());
    set(18, __DATE__ " " __TIME__);
    snprintf(v, sizeof(v), "%s / %d.%d.%d", esp_get_idf_version(), LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    set(19, v);
}

// Back to the quick-settings panel it was opened from.
void closeInfoPanel() {
    lv_obj_set_hidden(infoPanel, true);
    pendingQs = 1;
}

void onInfoGesture(lv_event_t*) {
    auto input = lv_indev_active();
    if (!input || lv_indev_get_gesture_dir(input) != LV_DIR_RIGHT) return;
    lv_indev_wait_release(input);
    closeInfoPanel();
}

void openInfoPanel() {
    if (qsPanel) lv_obj_set_hidden(qsPanel, true);
    refreshInfoPanel(); infoRefreshedAt = millis();
    { char line[32]; statusText(line, sizeof(line)); lv_label_set_text(infoStatus, line); }
    lv_obj_scroll_to_y(infoList, 0, LV_ANIM_OFF);
    lv_obj_set_hidden(infoPanel, false);
    lv_obj_move_foreground(infoPanel);
}

void buildInfoPanel() {
    infoPanel = box(lv_layer_top(), 0, 0, 320, 240, BG);
    lv_obj_set_style_radius(infoPanel, 0, 0);
    lv_obj_set_gesture_bubble(infoPanel, false);
    lv_obj_add_flag(infoPanel, LV_OBJ_FLAG_CLICKABLE);
    // Back: swipe right anywhere (no button needed).
    label(infoPanel, 12, 8, 180, "THÔNG TIN THIẾT BỊ", CYAN, &hud_font_vietnamese_14);
    infoStatus = label(infoPanel, 150, 8, 160, "", WHITE, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(infoStatus, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_event_cb(infoPanel, onInfoGesture, LV_EVENT_GESTURE, nullptr);
    infoList = box(infoPanel, 6, 32, 308, 206, BG);
    lv_obj_set_scrollable(infoList, true); lv_obj_set_scroll_dir(infoList, LV_DIR_VER);
    lv_obj_set_gesture_bubble(infoList, false);
    lv_obj_add_event_cb(infoList, onInfoGesture, LV_EVENT_GESTURE, nullptr);
    int y = 0;
    for (int i = 0; i < INFO_ROWS; ++i) {
        infoValues[i] = nullptr;
        if (INFO_LABELS[i][0] == '#') { // section header
            label(infoList, 6, y + (i ? 6 : 0), 290, INFO_LABELS[i] + 1, CYAN, &hud_font_vietnamese_14);
            y += i ? 28 : 22; continue;
        }
        auto row = box(infoList, 0, y, 296, 26, PANEL);
        lv_obj_set_style_radius(row, 8, 0);
        label(row, 8, 4, 120, INFO_LABELS[i], MUTED, &hud_font_vietnamese_14);
        infoValues[i] = label(row, 120, 4, 170, "", WHITE, &hud_font_vietnamese_14);
        lv_obj_set_style_text_align(infoValues[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(infoValues[i], LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(infoValues[i], 18);
        y += 30;
    }
    lv_obj_set_hidden(infoPanel, true);
}

void updateInfoPanel(uint32_t now) {
    if (!infoPanel || lv_obj_is_hidden(infoPanel)) return;
    if (hudLocked()) { lv_obj_set_hidden(infoPanel, true); return; }
    if (now - infoRefreshedAt >= 1000) { infoRefreshedAt = now; refreshInfoPanel(); }
}

// ---------------- Quick settings ----------------
constexpr uint16_t QS_TIMEOUTS[]={15,30,60,120,300,0};
void refreshQuickSettings() {
    if(!qsPanel) return;
    const auto& hw=hudHardwareSettingsGet();
    if(hudAutoBrightness()) lv_label_set_text_fmt(qsBrightValue,"Tự động · %u%%",unsigned(autoBrightnessLevel()));
    else lv_label_set_text_fmt(qsBrightValue,"%u%%",unsigned(hw.brightness));
    lv_obj_set_style_text_color(qsBrightValue,lv_color_hex(hudAutoBrightness() ? CYAN : WHITE),0);
    if(hw.speakerVolume) lv_label_set_text_fmt(qsVolumeValue,"%u%%",unsigned(hw.speakerVolume));
    else lv_label_set_text(qsVolumeValue,"Tắt tiếng");
    const uint16_t t=hudSettingsGet().timeoutSeconds;
    char timeout[24];
    if(!t) strlcpy(timeout,"Luôn sáng",sizeof(timeout));
    else if(t<60) snprintf(timeout,sizeof(timeout),"%u giây",unsigned(t));
    else snprintf(timeout,sizeof(timeout),"%u phút",unsigned(t/60));
    lv_label_set_text(qsTileValues[0],timeout);
    lv_label_set_text(qsTileValues[1],quietNotices ? "Im lặng" : "Bật");
    lv_label_set_text(qsTileValues[2],!phoneLinkEnabled() ? "Đã tắt" : linkSecured ? "iPhone" : "Chờ kết nối");
    const bool on[]={false,!quietNotices,phoneLinkEnabled()};
    for(int i=0;i<3;++i) {
        lv_obj_set_style_bg_color(qsTiles[i],lv_color_hex(on[i] ? CYAN : BUTTON),0);
        const uint32_t ink=on[i] ? BG : WHITE;
        lv_obj_set_style_text_color(lv_obj_get_child(qsTiles[i],0),lv_color_hex(on[i] ? BG : MUTED),0);
        lv_obj_set_style_text_color(qsTileValues[i],lv_color_hex(ink),0);
    }
}
void onQuickSetting(lv_event_t* event) {
    const int action=int(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    if(action<=3) {
        HudHardwareSettings hw=hudHardwareSettingsGet();
        // Manual +/- leaves automatic mode, starting from the level shown now.
        if(action<=1 && hudAutoBrightness()) { hw.brightness=autoBrightnessLevel(); hudSetAutoBrightness(false); }
        if(action==0) hw.brightness=uint8_t(std::max(10,int(hw.brightness)-10));
        if(action==1) hw.brightness=uint8_t(std::min(100,int(hw.brightness)+10));
        if(action==2) hw.speakerVolume=uint8_t(std::max(0,int(hw.speakerVolume)-10));
        if(action==3) hw.speakerVolume=uint8_t(std::min(100,int(hw.speakerVolume)+10));
        hudHardwareSettingsApply(hw);
    } else if(action==4) {
        HudSettings settings=hudSettingsGet();
        size_t index=0; while(index<6 && QS_TIMEOUTS[index]!=settings.timeoutSeconds) ++index;
        settings.timeoutSeconds=QS_TIMEOUTS[(index+1)%6];
        hudSettingsApply(settings);
    } else if(action==5) quietNotices=!quietNotices;
    else if(action==6) phoneLinkSetEnabled(!phoneLinkEnabled());
    else if(action==7) hudSetAutoBrightness(!hudAutoBrightness());
    else if(action==8) { openInfoPanel(); return; }
    refreshQuickSettings();
}
lv_obj_t* qsButton(lv_obj_t* parent,int x,int y,const char* text,int action) {
    auto button=lv_button_create(parent);
    lv_obj_set_pos(button,x,y); lv_obj_set_size(button,56,44);
    lv_obj_set_style_radius(button,22,0); lv_obj_set_style_shadow_width(button,0,0);
    lv_obj_set_style_bg_color(button,lv_color_hex(BUTTON),0);
    auto caption=label(button,0,0,40,text,WHITE,&lv_font_montserrat_22);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(caption);
    lv_obj_add_event_cb(button,onQuickSetting,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(action)));
    return button;
}
void buildQuickSettings() {
    qsPanel=box(lv_layer_top(),0,0,320,240,BG);
    lv_obj_set_style_radius(qsPanel,0,0);
    lv_obj_set_style_bg_opa(qsPanel,LV_OPA_COVER,0);
    lv_obj_set_gesture_bubble(qsPanel,false);
    label(qsPanel,12,5,170,"ĐIỀU KHIỂN NHANH",CYAN,&hud_font_vietnamese_14);
    const char* names[]={"Độ sáng","Âm lượng loa HUD"};
    lv_obj_t** values[]={&qsBrightValue,&qsVolumeValue};
    for(int r=0;r<2;++r) {
        auto row=box(qsPanel,10,26+r*54,300,50,PANEL); lv_obj_set_style_radius(row,14,0);
        qsButton(row,3,3,LV_SYMBOL_MINUS,r*2);
        if(r==0) { // tapping the middle of the brightness row toggles automatic mode
            auto middle=box(row,62,0,176,50,PANEL);
            lv_obj_add_flag(middle,LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(middle,onQuickSetting,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(7)));
        }
        auto name=label(row,62,4,176,r==0 ? "Độ sáng · chạm: tự động" : names[r],MUTED,&hud_font_vietnamese_14);
        lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_CENTER,0);
        *values[r]=label(row,62,22,176,"--",WHITE,textFont);
        lv_obj_set_style_text_align(*values[r],LV_TEXT_ALIGN_CENTER,0);
        qsButton(row,241,3,LV_SYMBOL_PLUS,r*2+1);
    }
    const char* tiles[]={"Tự tắt","Thông báo","BLE"};
    for(int i=0;i<3;++i) {
        qsTiles[i]=box(qsPanel,10+i*102,136,96,54,BUTTON);
        lv_obj_set_style_radius(qsTiles[i],14,0);
        lv_obj_add_flag(qsTiles[i],LV_OBJ_FLAG_CLICKABLE);
        label(qsTiles[i],10,5,80,tiles[i],MUTED,&hud_font_vietnamese_14);
        qsTileValues[i]=label(qsTiles[i],10,25,82,"",WHITE,textFont);
        lv_label_set_long_mode(qsTileValues[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(qsTileValues[i],24);
        lv_obj_add_event_cb(qsTiles[i],onQuickSetting,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(4+i)));
    }
    // Bottom row: device information.
    auto info=pillButton(qsPanel,10,194,300,44,LV_SYMBOL_SETTINGS,"Thông tin thiết bị",PANEL);
    lv_obj_add_event_cb(info,onQuickSetting,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(8)));
    qsStatus=label(qsPanel,150,5,160,"",WHITE,&hud_font_vietnamese_14);
    lv_obj_set_style_text_align(qsStatus,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_set_hidden(qsPanel,true);
}
void applyPendingQuickSettings() {
    const int request=pendingQs; pendingQs=0;
    if(!request || !qsPanel) return;
    if(request>0) { hidePopup(); refreshQuickSettings();
        { char line[32]; statusText(line,sizeof(line)); lv_label_set_text(qsStatus,line); } lv_obj_set_hidden(qsPanel,false); lv_obj_move_foreground(qsPanel); }
    else lv_obj_set_hidden(qsPanel,true);
}
// Classifies quick-settings contacts. Returns true when the contact belongs to it.
// A contact that starts in the top 48 px stays a normal touch (so nearby buttons
// still work) until it clearly drags down; then the pending press is cancelled
// and the drag belongs to quick settings.
constexpr int QS_EDGE=48, QS_CLAIM=14, QS_OPEN=36;
bool quickSettingsTouch(bool down,int x,int y,bool starting,bool wakeOnly) {
    static bool tracking=false,swallow=false; static int startX=0,startY=0;
    const bool open=qsPanel && !lv_obj_is_hidden(qsPanel);
    if(starting) {
        startX=x; startY=y; swallow=false;
        // Not on the AI page: there swipe-down from the top closes AI.
        tracking=!open && !wakeOnly && y<=QS_EDGE && currentPage!=AI_PAGE &&
            !(pairingPanel && lv_obj_is_visible(pairingPanel)) && !(pinPanel && lv_obj_is_visible(pinPanel)) && !(infoPanel && lv_obj_is_visible(infoPanel));
    }
    if(tracking) {
        const int dx=x-startX, dy=y-startY;
        if(!down) { tracking=false; return false; }
        if(dy<-6 || abs(dx)>dy+10) { tracking=false; return false; }   // not a downward drag
        if(dy>=QS_CLAIM) {
            if(auto input=lv_indev_active()) lv_indev_reset(input,nullptr);   // no click on what was under the finger
            if(dy>=QS_OPEN) { pendingQs=1; tracking=false; swallow=true; }
            return true;
        }
        return false;
    }
    if(swallow) { if(!down) swallow=false; return true; }
    if(open) {
        if(down && startY-y>=40) { pendingQs=-1; swallow=true; if(auto input=lv_indev_active()) lv_indev_reset(input,nullptr); return true; }
        return false;   // buttons and tiles get normal LVGL presses
    }
    return false;
}
}

bool companionUiTouch(bool down,int x,int y,bool wakeOnly,int touches,int x2,int y2) {
    if(!root || !aiListening) return false;
    if(currentPage==2 && mapLive && mapPanMode && !wakeOnly) {
        if(down && touches==2) {
            const float dx=float(x2-x), dy=float(y2-y);
            const float dist=std::max(20.0f,sqrtf(dx*dx+dy*dy)), angle=atan2f(dy,dx)*57.29578f;
            if(!mapPinch.active) {
                mapPinch.startDist=dist; mapPinch.startAngle=angle; mapPinch.scale=1; mapPinch.angle=0;
                mapPinch.ended=false; mapPinch.active=true;
            } else {
                mapPinch.scale=std::max(0.33f,std::min(3.0f,dist/mapPinch.startDist));
                float turn=angle-mapPinch.startAngle;
                while(turn>180) turn-=360;
                while(turn<-180) turn+=360;
                mapPinch.angle=turn;
            }
            return true;   // LVGL sees a release; the one-finger drag stops here
        }
        if(mapPinch.active) {   // swallow until every finger is up
            if(!down) { mapPinch.active=false; mapPinch.ended=true; }
            return true;
        }
    }
    if(hudLocked()) {
        // Locked: the lock sheet and PIN keypad take normal taps; anything else
        // only opens the lock sheet on release.
        if((lockSheet && !lv_obj_is_hidden(lockSheet)) || (pinPanel && !lv_obj_is_hidden(pinPanel))) return false;
        static bool lockedDown=false;
        if(!down && lockedDown && !wakeOnly) pendingLockTap=true;
        lockedDown=down;
        return true;
    }
    {
        static bool qsPreviousDown=false;
        const bool starting=down && !qsPreviousDown; qsPreviousDown=down;
        if(quickSettingsTouch(down,x,y,starting,wakeOnly)) return true;
        if(qsPanel && !lv_obj_is_hidden(qsPanel)) return false;
    }
    // Keep the AI drawer reachable from destination results/confirmation.
    // Remember where this contact began so moving upwards does not suddenly
    // turn the same edge swipe into a blocked modal contact.
    static bool previousDown=false,mapSearchEdge=false;
    if(down && !previousDown) mapSearchEdge=mapSearchPanel && lv_obj_is_visible(mapSearchPanel) && y>=viewHeight-40;
    const bool blocked=wakeOnly || (pairingPanel && lv_obj_is_visible(pairingPanel)) || (pinPanel && lv_obj_is_visible(pinPanel)) || (infoPanel && lv_obj_is_visible(infoPanel)) ||
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

void refreshPinPanel(const char* hint = nullptr) {
    char dots[16] = {};
    for (int i = 0; i < 4; ++i) strlcat(dots, i < int(strlen(pinTyped)) ? "* " : "- ", sizeof(dots));
    lv_label_set_text(pinDots, dots);
    if (hint) lv_label_set_text(pinHint, hint);
}

void showPinPanel() {
    pinTyped[0] = 0;
    const bool locked = pinLockedUntil && int32_t(pinLockedUntil - millis()) > 0;
    refreshPinPanel(locked ? "Nhập sai nhiều lần. Thử lại sau 5 phút." :
                    pinForUnlock ? "Nhập mã PIN chủ xe để mở khóa HUD." : "iPhone lạ đang xin kết nối. Nhập mã PIN chủ xe.");
    lv_obj_set_hidden(pinPanel, false);
    lv_obj_move_foreground(pinPanel);
}

void onPinKey(lv_event_t* event) {
    const int key = int(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    if (pinLockedUntil && int32_t(pinLockedUntil - millis()) > 0) { refreshPinPanel("Nhập sai nhiều lần. Thử lại sau 5 phút."); return; }
    const size_t n = strlen(pinTyped);
    if (key == 10) { lv_obj_set_hidden(pinPanel, true); pinTyped[0] = 0; pinForUnlock = false; return; }   // Hủy
    if (key == 11) { if (n) pinTyped[n - 1] = 0; refreshPinPanel(); return; }        // Xóa
    if (n >= 4) return;
    pinTyped[n] = char('0' + key); pinTyped[n + 1] = 0;
    refreshPinPanel();
    if (n + 1 < 4) return;
    if (hudOwnerPinMatches(pinTyped) && pinForUnlock) {
        pinFailures = 0; pinForUnlock = false; pinUnlocked = true; pinTyped[0] = 0;
        lv_obj_set_hidden(pinPanel, true); lv_obj_set_hidden(lockSheet, true);
        showPage(currentPage);
        return;
    }
    if (hudOwnerPinMatches(pinTyped)) {
        pinFailures = 0;
        pairUnlockedUntil = (millis() + 180000) | 1;
        lv_obj_set_hidden(pinPanel, true);
        if (pendingPasskeyValid) {
            lv_label_set_text_fmt(pairingCode, "%06u", unsigned(pendingPasskey));
            lv_obj_set_hidden(pairingPanel, false);
        }
        pinTyped[0] = 0;
        return;
    }
    pinTyped[0] = 0;
    if (++pinFailures >= 5) { pinFailures = 0; pinLockedUntil = (millis() + 300000) | 1; refreshPinPanel("Nhập sai nhiều lần. Thử lại sau 5 phút."); }
    else { char text[64]; snprintf(text, sizeof(text), "Sai mã PIN. Còn %u lần thử.", unsigned(5 - pinFailures)); refreshPinPanel(text); }
}

void buildPinPanel() {
    pinPanel = box(lv_layer_top(), 0, 0, 320, 240, BG);
    lv_obj_set_style_radius(pinPanel, 0, 0);
    lv_obj_set_gesture_bubble(pinPanel, false);
    lv_obj_add_flag(pinPanel, LV_OBJ_FLAG_CLICKABLE); // swallow touches meant for the pages below
    label(pinPanel, 12, 4, 200, "MÃ PIN CHỦ XE", CYAN, &hud_font_vietnamese_14);
    pinHint = label(pinPanel, 12, 22, 200, "", MUTED, &hud_font_vietnamese_14);
    lv_label_set_long_mode(pinHint, LV_LABEL_LONG_MODE_WRAP);
    pinDots = label(pinPanel, 12, 72, 200, "", WHITE, &lv_font_montserrat_34);
    label(pinPanel, 12, 120, 196, "Quên mã: xóa mã trong app HUD Connect trên iPhone đã ghép.", MUTED, &hud_font_vietnamese_14);
    // 3x4 keypad on the right: 1-9, Hủy, 0, Xóa
    const char* caps[] = {"1","2","3","4","5","6","7","8","9","Hủy","0",LV_SYMBOL_BACKSPACE};
    const int keys[] = {1,2,3,4,5,6,7,8,9,10,0,11};
    for (int i = 0; i < 12; ++i) {
        auto button = lv_button_create(pinPanel);
        lv_obj_set_pos(button, 214 + (i % 3) * 35, 6 + (i / 3) * 58);
        lv_obj_set_size(button, 33, 52);
        lv_obj_set_style_radius(button, 10, 0); lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(i == 9 ? 0x5A2330 : BUTTON), 0);
        auto caption = label(button, 0, 0, 33, caps[i], WHITE, i == 9 ? &hud_font_vietnamese_14 : &lv_font_montserrat_22);
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(caption);
        lv_obj_add_event_cb(button, onPinKey, LV_EVENT_CLICKED, reinterpret_cast<void*>(intptr_t(keys[i])));
    }
    lv_obj_set_hidden(pinPanel, true);
}

void onLockPin(lv_event_t*) {
    lv_obj_set_hidden(lockSheet, true);
    pinForUnlock = true;
    showPinPanel();
}

void buildLockSheet() {
    lockSheet = box(lv_layer_top(), 0, 0, 320, 240, BG);
    lv_obj_set_style_radius(lockSheet, 0, 0);
    lv_obj_set_gesture_bubble(lockSheet, false);
    lv_obj_add_flag(lockSheet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lockSheet, [](lv_event_t*) { lv_obj_set_hidden(lockSheet, true); }, LV_EVENT_CLICKED, nullptr);
    auto icon = label(lockSheet, 0, 22, 320, LV_SYMBOL_BLUETOOTH, CYAN, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
    auto title = label(lockSheet, 0, 70, 320, "HUD đã khóa", WHITE, &hud_font_vietnamese_24);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    auto body = label(lockSheet, 20, 104, 280, "Mở bằng iPhone của bạn: bật Bluetooth và HUD Connect, HUD sẽ tự mở.", MUTED, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lockPinButton = lv_button_create(lockSheet);
    lv_obj_set_pos(lockPinButton, 80, 160); lv_obj_set_size(lockPinButton, 160, 48);
    lv_obj_set_style_radius(lockPinButton, 24, 0); lv_obj_set_style_shadow_width(lockPinButton, 0, 0);
    lv_obj_set_style_bg_color(lockPinButton, lv_color_hex(CYAN), 0);
    auto caption = label(lockPinButton, 0, 0, 150, "Nhập mã PIN", BG, &hud_font_vietnamese_24);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(caption);
    lv_obj_add_event_cb(lockPinButton, onLockPin, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_hidden(lockSheet, true);
}

void updateLock(uint32_t now) {
    static bool wasLocked = false;
    const bool locked = hudLocked();
    if (locked != wasLocked) {
        wasLocked = locked;
        if (locked) {
            if (qsPanel) lv_obj_set_hidden(qsPanel, true);
            aiHolding = false; lv_obj_set_hidden(aiListening, true);
            detailOpen = false;
            if (currentPage != 0) showPage(0);
        } else {
            lv_obj_set_hidden(lockSheet, true);
            if (!pendingPasskeyValid) lv_obj_set_hidden(pinPanel, true);
        }
        if (currentPage == 0) showPage(0); // refresh the heading
    }
    if (locked && pendingLockTap) {
        pendingLockTap = false;
        lv_obj_set_hidden(lockPinButton, !hudOwnerPinSet());
        lv_obj_set_hidden(lockSheet, false); lv_obj_move_foreground(lockSheet);
        lockSheetUntil = now + 8000;
    }
    pendingLockTap = false;
    if (lockSheetUntil && int32_t(now - lockSheetUntil) >= 0) { lockSheetUntil = 0; lv_obj_set_hidden(lockSheet, true); }
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
    constexpr size_t motionPixels=MAP_W*(MAP_H+2*hudnav::MAX_MOTION_MARGIN);
    static_assert(motionPixels >= (MAP_W+2*hudnav::LEGACY_MOTION_MARGIN)*(MAP_H+2*hudnav::LEGACY_MOTION_MARGIN),
                  "Map buffer must fit legacy frames");
    for(auto& buffer:mapPixels) buffer=static_cast<uint16_t*>(heap_caps_malloc(motionPixels*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(mapPixels[0] && mapPixels[1])
        xTaskCreatePinnedToCore(mapDecodeWorker,"map-jpeg",8192,nullptr,1,&mapDecodeTask,0);
    else Serial.println("Map decode buffers unavailable");
    root = box(nullptr, 0, 0, 320, 240, BG);
    lv_obj_set_gesture_bubble(root, false);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_add_event_cb(root, onGesture, LV_EVENT_GESTURE, nullptr);
    // Header: page title left, clock centred, battery + link dot right (no page dots).
    pageHeading = label(root, 12, 4, 112, "ĐỒNG HỒ", CYAN);
    lv_label_set_long_mode(pageHeading, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(pageHeading, 24);
    for (int i = 0; i < PAGE_COUNT; ++i) {
        pageDots[i] = box(root, 160 + i * 10, 15, 4, 4, MUTED);
        lv_obj_set_style_radius(pageDots[i], LV_RADIUS_CIRCLE, 0);
    }
    smallClock=label(root,124,5,72,"--:--",WHITE,&lv_font_montserrat_18);
    lv_obj_set_style_text_align(smallClock,LV_TEXT_ALIGN_CENTER,0);
    batteryLabel=label(root,240,7,62,"",MUTED,&lv_font_montserrat_12);
    lv_obj_set_style_text_align(batteryLabel,LV_TEXT_ALIGN_RIGHT,0);
    statusDot = box(root, 306, 12, 7, 7, MUTED);
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

    rideFace = box(pages[0], 0, 0, 300, 200, PANEL);
    rideSpeed = label(rideFace, 2, -4, 160, "--", WHITE, &hud_font_clock_80);
    rideUnit = label(rideFace, 6, 84, 100, "KM/H", MUTED, &hud_font_vietnamese_14);
    rideClock = label(rideFace, 140, 0, 156, "--:--", WHITE, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(rideClock, LV_TEXT_ALIGN_RIGHT, 0);
    rideDate = label(rideFace, 120, 38, 176, "", MUTED, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(rideDate, LV_TEXT_ALIGN_RIGHT, 0);
    rideWeather = label(rideFace, 110, 58, 186, "", MUTED, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(rideWeather, LV_TEXT_ALIGN_RIGHT, 0);
    rideLeftTitle = label(rideFace, 120, 82, 176, "Còn lại", MUTED, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(rideLeftTitle, LV_TEXT_ALIGN_RIGHT, 0);
    rideLeft = label(rideFace, 120, 100, 176, "--", WHITE, &hud_font_vietnamese_24);
    lv_obj_set_style_text_align(rideLeft, LV_TEXT_ALIGN_RIGHT, 0);
    rideCard = box(rideFace, 0, 136, 300, 64, BUTTON);
    lv_obj_set_style_radius(rideCard, 14, 0);
    // Tap the turn card to open the map page (also from "Chưa có chuyến đi" to search).
    lv_obj_add_flag(rideCard, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(rideCard, lv_color_hex(OVERLAY), LV_STATE_PRESSED);
    lv_obj_add_event_cb(rideCard, [](lv_event_t*) { if (!hudLocked()) showPage(2); }, LV_EVENT_CLICKED, nullptr);
    rideArrow = box(rideCard, 8, 9, 46, 46, 0x45D483);
    lv_obj_set_style_radius(rideArrow, 12, 0);
    rideArrowIcon = label(rideArrow, 0, 0, 46, LV_SYMBOL_GPS, 0x04170C, &lv_font_montserrat_22);
    lv_obj_set_style_text_align(rideArrowIcon, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(rideArrowIcon);
    rideTurnIcon = makeTurnIcon(rideArrow, rideArrowIcon, 0x04170C);
    setTurnIcon(rideTurnIcon, -1);
    rideTurn = label(rideCard, 64, 4, 228, "", WHITE, &hud_font_vietnamese_24);
    lv_label_set_long_mode(rideTurn, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rideTurn, 28);
    rideStreet = label(rideCard, 64, 36, 228, "", MUTED, &hud_font_vietnamese_14);
    lv_label_set_long_mode(rideStreet, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rideStreet, 20);
    rideThen = label(rideCard,64,46,228,"",0x8BDCF7,&hud_font_vietnamese_14);
    lv_label_set_long_mode(rideThen,LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rideThen,18);
    lv_obj_set_hidden(rideThen,true);
    lv_obj_set_hidden(rideFace, true);

    inboxCount=label(pages[1],8,2,205,"0 thông báo",WHITE);
    refreshButton=iconButton(pages[1],220,0,34,LV_SYMBOL_REFRESH,onRefresh);
    refreshCaption=lv_obj_get_child(refreshButton,0);
    inboxDeleteAll=iconButton(pages[1],262,0,34,LV_SYMBOL_TRASH,askDelete,reinterpret_cast<void*>(1));
    inboxList=box(pages[1],0,34,300,166,PANEL);
    lv_obj_set_scrollable(inboxList,true); lv_obj_set_scroll_dir(inboxList,LV_DIR_VER);
    // Rows: app badge, sender/title, "App · preview", time. 58 px tall, 6 px apart.
    for (int i=0;i<10;++i) {
        auto row=box(inboxList,4,i*64,287,58,BUTTON); inboxRows[i]=row;
        lv_obj_set_style_radius(row,12,0);
        lv_obj_set_clickable(row,true);
        lv_obj_add_event_cb(row,openNotice,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(i)));
        rowBadges[i]=box(row,8,13,32,32,CYAN); lv_obj_set_style_radius(rowBadges[i],LV_RADIUS_CIRCLE,0);
        lv_obj_remove_flag(rowBadges[i],LV_OBJ_FLAG_CLICKABLE);
        rowBadgeText[i]=label(rowBadges[i],0,0,32,"",0x061016,textFont);
        lv_obj_set_style_text_align(rowBadgeText[i],LV_TEXT_ALIGN_CENTER,0); lv_obj_align(rowBadgeText[i],LV_ALIGN_CENTER,0,0);
        rowTitles[i]=label(row,48,5,186,"",WHITE,textFont);
        lv_label_set_long_mode(rowTitles[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rowTitles[i],24);
        rowTimes[i]=label(row,236,8,44,"",MUTED,&lv_font_montserrat_12);
        lv_obj_set_style_text_align(rowTimes[i],LV_TEXT_ALIGN_RIGHT,0);
        rowContents[i]=label(row,48,31,232,"",MUTED,&hud_font_vietnamese_14);
        lv_label_set_long_mode(rowContents[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(rowContents[i],20);
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
    lv_obj_set_size(mapImage, MAP_W, MAP_H);
    lv_obj_set_clickable(mapImage, false);
    lv_obj_set_hidden(mapImage, true);
    mapVehicleDot=box(pages[2],153,113,14,14,0xFFFFFF);
    lv_obj_set_style_radius(mapVehicleDot,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_border_width(mapVehicleDot,2,0);
    lv_obj_set_style_border_color(mapVehicleDot,lv_color_hex(0x142132),0);
    lv_obj_set_clickable(mapVehicleDot,false);
    mapVehicleArrow=label(pages[2],148,108,24,LV_SYMBOL_UP,MAP_ARROW_ON_DARK,&lv_font_montserrat_22);
    lv_obj_set_style_text_align(mapVehicleArrow,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_hidden(mapVehicleDot,true);
    lv_obj_set_hidden(mapVehicleArrow,true);
    // Keep the hit target fixed even when the image moves during a drag.
    mapTouchSurface = box(pages[2],0,0,MAP_W,MAP_H,BG);
    lv_obj_set_style_bg_opa(mapTouchSurface,LV_OPA_TRANSP,0);
    lv_obj_set_clickable(mapTouchSurface,true);
    lv_obj_set_press_lock(mapTouchSurface,true);
    lv_obj_add_event_cb(mapTouchSurface,onMapTouch,LV_EVENT_ALL,nullptr);
    lv_obj_set_hidden(mapTouchSurface,true);
    googleOverlay = box(pages[2], 4, 4, 292, 60, 0x080E18);
    lv_obj_set_style_bg_opa(googleOverlay, LV_OPA_90, 0);
    googleHeading = label(googleOverlay, 6, 2, 280, "Google Maps", WHITE, &hud_font_vietnamese_24);
    lv_label_set_long_mode(googleHeading, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(googleHeading, 31);
    googleText = label(googleOverlay, 6, 32, 280, "", WHITE);
    lv_label_set_long_mode(googleText, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(googleText, 26);
    lv_obj_set_hidden(googleOverlay, true);
    // Hidden legacy speed pill (kept so older update paths stay valid).
    speedBox=box(pages[2],152,164,104,30,OVERLAY);
    speedLabel=label(speedBox,4,5,96,"--- km/h",WHITE,&lv_font_montserrat_18);
    lv_obj_set_hidden(speedBox,true);
    buildNavOverlay();
    // Control dock (tap the map to show it, hides after 5 s): hand-control toggle,
    // search, back to the bike. Zoom/pan/rotate are done with the fingers.
    mapControls=box(pages[2],6,130,308,66,0x15191F);
    lv_obj_set_style_bg_opa(mapControls,235,0);
    lv_obj_set_style_radius(mapControls,14,0);
    lv_obj_set_style_border_width(mapControls,1,0);
    lv_obj_set_style_border_color(mapControls,lv_color_hex(0x343C46),0);
    auto smallPill=[](lv_obj_t* parent,int x,const char* icon,const char* text) {
        auto b=pillButton(parent,x,4,96,40,icon,text,BUTTON);
        lv_obj_set_style_radius(b,12,0);
        lv_obj_set_style_text_font(lv_obj_get_child(b,lv_obj_get_child_count(b)-1),&hud_font_vietnamese_14,0);
        return b;
    };
    mapPanButton=smallPill(mapControls,6,nullptr,"Điều khiển");
    lv_obj_set_style_bg_color(mapPanButton,lv_color_hex(0x1F6F8B),LV_STATE_CHECKED);
    lv_obj_set_style_border_width(mapPanButton,2,LV_STATE_CHECKED);
    lv_obj_set_style_border_color(mapPanButton,lv_color_hex(CYAN),LV_STATE_CHECKED);
    lv_obj_add_event_cb(mapPanButton,onMapControl,LV_EVENT_CLICKED,reinterpret_cast<void*>(5));
    auto dockSearch=smallPill(mapControls,106,nullptr,"Tìm đường");
    lv_obj_add_event_cb(dockSearch,openMapSearch,LV_EVENT_CLICKED,nullptr);
    auto dockHome=smallPill(mapControls,206,LV_SYMBOL_GPS,"Về xe");
    lv_obj_add_event_cb(dockHome,onMapControl,LV_EVENT_CLICKED,reinterpret_cast<void*>(3));
    // Hand-control mode: one small chip to leave it (dock hidden so fingers reach the map).
    mapDoneButton=pillButton(pages[2],6,162,90,34,LV_SYMBOL_CLOSE,"Xong",0x15191F);
    lv_obj_set_style_border_width(mapDoneButton,1,0);
    lv_obj_set_style_border_color(mapDoneButton,lv_color_hex(CYAN),0);
    lv_obj_set_style_text_font(lv_obj_get_child(mapDoneButton,1),&hud_font_vietnamese_14,0);
    lv_obj_set_style_text_font(lv_obj_get_child(mapDoneButton,0),&lv_font_montserrat_14,0);
    lv_obj_add_event_cb(mapDoneButton,onMapControl,LV_EVENT_CLICKED,reinterpret_cast<void*>(5));
    lv_obj_set_hidden(mapDoneButton,true);
    mapZoomOut=iconButton(pages[2],6,162,38,LV_SYMBOL_MINUS,onMapControl,reinterpret_cast<void*>(2));
    mapZoomIn=iconButton(pages[2],50,162,38,LV_SYMBOL_PLUS,onMapControl,reinterpret_cast<void*>(1));
    lv_obj_set_hidden(mapZoomIn,true);
    lv_obj_set_hidden(mapZoomOut,true);
    mapStaleLabel=label(pages[2],6,105,184,"Đang cập nhật bản đồ…",WHITE,&hud_font_vietnamese_14);
    lv_obj_set_style_bg_color(mapStaleLabel,lv_color_hex(0x15191F),0);
    lv_obj_set_style_bg_opa(mapStaleLabel,LV_OPA_90,0);
    lv_obj_set_hidden(mapStaleLabel,true);
    auto hint=label(mapControls,0,46,308,"Giữ màn hình 3 giây để kết thúc chuyến",0x98A2AD,&hud_font_vietnamese_14);
    lv_obj_set_style_text_align(hint,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_hidden(mapControls,true);
    mapSearchButton=HudDestinationUi::searchButton(pages[2],262,3,BUTTON,CYAN,openMapSearch);
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

    // Music: source + state on top, big title, volume row, three large transport buttons.
    label(pages[3], 10, 9, 18, LV_SYMBOL_AUDIO, CYAN, &lv_font_montserrat_14);
    musicPlayer = label(pages[3], 30, 6, 150, "Nhạc iPhone", MUTED, &hud_font_vietnamese_14);
    lv_label_set_long_mode(musicPlayer, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(musicPlayer, 20);
    musicStatus = label(pages[3], 170, 6, 120, "", CYAN, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(musicStatus, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(musicStatus, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(musicStatus, 20);
    musicTitle = label(pages[3], 10, 30, 280, "Mở nhạc trên iPhone", WHITE, &hud_font_vietnamese_24);
    lv_label_set_long_mode(musicTitle, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    musicArtist = label(pages[3], 10, 63, 280, "", MUTED);
    lv_label_set_long_mode(musicArtist, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    musicVolumeBar = lv_bar_create(pages[3]);
    lv_obj_set_pos(musicVolumeBar, 62, 111); lv_obj_set_size(musicVolumeBar, 138, 8);
    lv_bar_set_range(musicVolumeBar, 0, 100);
    lv_obj_set_style_bg_color(musicVolumeBar, lv_color_hex(BUTTON), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(musicVolumeBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(musicVolumeBar, lv_color_hex(CYAN), LV_PART_INDICATOR);
    musicVolume = label(pages[3], 202, 105, 44, "--%", WHITE, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(musicVolume, LV_TEXT_ALIGN_CENTER, 0);
    const char* controls[] = {LV_SYMBOL_MINUS, LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT, LV_SYMBOL_PLUS};
    const int bx[] = {10, 10, 100, 210, 248}, by[] = {98, 142, 142, 142, 98};
    const int bw[] = {44, 80, 100, 80, 44}, bh[] = {34, 52, 52, 52, 34};
    for (int i = 0; i < MUSIC_BUTTON_COUNT; ++i) {
        musicButtons[i] = lv_button_create(pages[3]);
        lv_obj_set_pos(musicButtons[i], bx[i], by[i]);
        lv_obj_set_size(musicButtons[i], bw[i], bh[i]);
        lv_obj_set_style_radius(musicButtons[i], bh[i] / 2, 0);
        lv_obj_set_style_shadow_width(musicButtons[i], 0, 0);
        lv_obj_set_style_bg_color(musicButtons[i], lv_color_hex(i == 2 ? CYAN : BUTTON), 0);
        lv_obj_set_style_pad_all(musicButtons[i], 0, 0);
        auto icon = label(musicButtons[i], 0, 0, 40, controls[i], i == 2 ? BG : WHITE,
                          i == 0 || i == 4 ? &lv_font_montserrat_18 : &lv_font_montserrat_24);
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

    // Compact pill under the status bar (Dynamic Island style): badge + one line of text,
    // sized to its content. Tap opens the message; it hides itself after a few seconds.
    noticePopup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(noticePopup);
    lv_obj_set_size(noticePopup, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_max_width(noticePopup, 300, 0);
    lv_obj_set_style_bg_color(noticePopup, lv_color_hex(0x1F242B), 0);
    lv_obj_set_style_bg_opa(noticePopup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(noticePopup, 1, 0);
    lv_obj_set_style_border_color(noticePopup, lv_color_hex(0x3A424D), 0);
    lv_obj_set_style_radius(noticePopup, 16, 0);
    lv_obj_set_style_pad_left(noticePopup, 4, 0);
    lv_obj_set_style_pad_right(noticePopup, 14, 0);
    lv_obj_set_flex_flow(noticePopup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(noticePopup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(noticePopup, 8, 0);
    lv_obj_set_scrollable(noticePopup, false);
    lv_obj_add_flag(noticePopup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_event_bubble(noticePopup, false);
    lv_obj_add_event_cb(noticePopup, onPopup, LV_EVENT_CLICKED, nullptr);
    lv_obj_align(noticePopup, LV_ALIGN_TOP_MID, 0, 30);
    popupBadge = lv_obj_create(noticePopup);
    lv_obj_remove_style_all(popupBadge);
    lv_obj_set_size(popupBadge, 24, 24);
    lv_obj_set_style_radius(popupBadge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(popupBadge, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(popupBadge, lv_color_hex(0x3FA2F5), 0);
    lv_obj_remove_flag(popupBadge, LV_OBJ_FLAG_CLICKABLE);
    popupApp = lv_label_create(popupBadge);
    lv_obj_set_style_text_font(popupApp, &hud_font_vietnamese_14, 0);
    lv_obj_set_style_text_color(popupApp, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(popupApp, "");
    lv_obj_center(popupApp);
    popupText = lv_label_create(noticePopup);
    lv_obj_set_style_text_font(popupText, &hud_font_vietnamese_14, 0);
    lv_obj_set_style_text_color(popupText, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(popupText, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(popupText, 250, 0);
    lv_obj_set_height(popupText, 18);
    lv_label_set_long_mode(popupText, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(popupText, "");
    popupCloseButton = nullptr;
    hidePopup();
    pairingPanel = box(lv_layer_top(), 12, 38, 296, 154, 0x173347);
    lv_obj_set_style_border_width(pairingPanel, 2, 0);
    lv_obj_set_style_border_color(pairingPanel, lv_color_hex(CYAN), 0);
    label(pairingPanel, 12, 5, 270, "Nhập mã này trên iPhone", CYAN);
    pairingCode = label(pairingPanel, 0, 40, 292, "000000", WHITE, &lv_font_montserrat_34);
    lv_obj_set_style_text_align(pairingCode, LV_TEXT_ALIGN_CENTER, 0);
    label(pairingPanel, 12, 91, 270, "Sau đó cho phép chia sẻ thông báo hệ thống.", MUTED);
    lv_obj_set_hidden(pairingPanel, true);
    buildQuickSettings();
    buildPinPanel();
    buildInfoPanel();
    buildWallFace();
    buildLockSheet();
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
        if (hudOwnerPinSet() && !(pairUnlockedUntil && int32_t(pairUnlockedUntil - millis()) > 0)) {
            // A new iPhone is asking to pair: the code stays hidden until the owner PIN is typed.
            pendingPasskey = event.value; pendingPasskeyValid = true;
            pinForUnlock = false; lv_obj_set_hidden(lockSheet, true);
            showPinPanel();
        } else lv_obj_set_hidden(pairingPanel, false);
        wakeRequested = true;
        break;
    case PhoneEventType::Secured:
        linkSecured=true; pinUnlocked=false;
        status(0x69E4AA);
        lv_obj_set_hidden(pairingPanel, true);
        lv_obj_set_hidden(pinPanel, true); pendingPasskeyValid = false;
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
        if(!event.notice.preExisting && !quietNotices) wakeRequested=true;
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
        for(size_t i=0;i<count;++i) if(notices[i].uid==popupUid) setPopupBadge(notices[i].app,appName(notices[i].app));
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
            setFontSafeText(musicTitle, event.music.text[0] ? event.music.text : "Chưa có bài nhạc", true);
            break;
        case phone::MusicField::Artist:
            setFontSafeText(musicArtist, event.music.text, true);
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
                lv_label_set_text_fmt(musicVolume, "%d%%", musicVolumePercent);
                lv_bar_set_value(musicVolumeBar, musicVolumePercent, LV_ANIM_ON);
            }
            break;
        }
        case phone::MusicField::Player:
            lv_label_set_text(musicPlayer, event.music.text[0] ? event.music.text : "Nhạc iPhone");
            break;
        }
        break;
    case PhoneEventType::MusicCommandFailed:
        musicHint("iPhone chưa nhận lệnh");
        break;
    case PhoneEventType::Disconnected:
        linkSecured=false;
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
        lv_obj_set_hidden(pinPanel, true); pendingPasskeyValid = false;
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

void formatDistance(char* out, size_t size, uint32_t metres) {
    if (metres < 1000) snprintf(out, size, "%u m", unsigned(metres < 100 ? metres : metres / 10 * 10));
    else if (metres < 10000) snprintf(out, size, "%u,%u km", unsigned(metres / 1000), unsigned(metres % 1000 / 100));
    else snprintf(out, size, "%u km", unsigned((metres + 500) / 1000));
}

// "12,4 km" ridden today (phone GPS), "--" when unknown or from another day.
bool todayText(char* out, size_t size) {
    TodayDistance today{};
    if (!navStreamToday(today) || !clockValid) { strlcpy(out, "--", size); return false; }
    const time_t local = time(nullptr) + utcOffset; tm d{}; gmtime_r(&local, &d);
    const uint32_t key = uint32_t(d.tm_year + 1900) * 10000 + (d.tm_mon + 1) * 100 + d.tm_mday;
    formatDistance(out, size, today.dayKey == key ? today.metres : 0);
    return true;
}

// Rain line shared by the riding face and the trip-start warning.
bool weatherText(char* out, size_t size, bool& rainy) {
    WeatherInfo w{};
    rainy = false;
    if (!navStreamWeather(w)) { out[0] = 0; return false; }
    const int temp = (w.tempTenths + (w.tempTenths >= 0 ? 5 : -5)) / 10;
    rainy = w.rainChance >= 40 && w.rainInMinutes != 0xFFFF;
    if (!rainy) snprintf(out, size, "%d° · Không mưa", temp);
    else if (w.rainInMinutes < 30) snprintf(out, size, "Đang/sắp mưa %u%%", unsigned(w.rainChance));
    else if (clockValid) {
        const time_t at = time(nullptr) + utcOffset + w.rainInMinutes * 60; tm d{}; gmtime_r(&at, &d);
        snprintf(out, size, "Mưa %u%% lúc %dh", unsigned(w.rainChance), d.tm_hour);
    } else snprintf(out, size, "Mưa %u%% trong %u phút", unsigned(w.rainChance), unsigned(w.rainInMinutes));
    return true;
}

void showRainWarning(const char* text) {
    if (quietNotices || lv_obj_is_visible(pairingPanel)) return;
    popupUid = 0;
    setPopupBadge("weather", "!", 0xF5A623);
    char popup[160];
    snprintf(popup, sizeof(popup), "%s · nhớ mang áo mưa", text);
    lv_label_set_text(popupText, popup);
    revealPopup(10000);
    wakeRequested = true;
}

void updateRideFace(uint32_t now) {
    static uint32_t last = 0;
    if (now - last < 250) return;
    last = now;
    TripInfo trip{};
    const bool tripActive = navStreamTrip(trip) || mapTripActive;
    // Trips always switch to the ride face; it can also be the chosen default face.
    const bool active = tripActive || hudSettingsGet().face == RIDE_FACE;
    if (tripActive != rideTrip) {
        rideTrip = tripActive;
        if (tripActive && !rainWarned) {
            char text[64]; bool rainy = false;
            if (weatherText(text, sizeof(text), rainy) && rainy) showRainWarning(text);
            rainWarned = true;
        }
        if (!tripActive) rainWarned = false;
    }
    if (active != rideVisible) {
        rideVisible = active;
        lv_obj_set_hidden(rideFace, !active);
        if (active) lv_obj_move_foreground(rideFace);
        if (currentPage == 0) lv_label_set_text(pageHeading, hudLocked() ? "ĐÃ KHÓA" : active ? "ĐI XE" : "ĐỒNG HỒ");
    }
    if (!active) return;
    const int tenths = navStreamSpeedTenths();
    char text[96]; bool rainy = false;
    if (tenths >= 0) snprintf(text, sizeof(text), "%d", (tenths + 5) / 10); else strlcpy(text, "--", sizeof(text));
    setIfChanged(rideSpeed, text);
    textColorIfChanged(rideSpeed, tenths > 600 ? 0xFF5A79 : WHITE);
    weatherText(text, sizeof(text), rainy);
    setIfChanged(rideWeather, text);
    textColorIfChanged(rideWeather, rainy ? 0xF5B83D : MUTED);
    bgColorIfChanged(rideArrow, tripActive ? 0x45D483 : BUTTON);
    textColorIfChanged(rideArrowIcon, tripActive ? 0x04170C : MUTED);
    const bool twoTurns = trip.active && trip.followingManeuver != 255;
    lv_obj_set_y(rideStreet, twoTurns ? 29 : 36);
    showFollowingTurn(rideThen, trip, twoTurns);
    if (!tripActive) {
        // Default ride face without a trip: today's distance and a hint to start one.
        setIfChanged(rideLeftTitle, "Hôm nay");
        todayText(text, sizeof(text));
        setIfChanged(rideLeft, text);
        setIfChanged(rideTurn, "Chưa có chuyến đi");
        setIfChanged(rideStreet, "Vuốt sang Bản đồ để chọn điểm đến");
        setIfChanged(rideArrowIcon, LV_SYMBOL_GPS); setTurnIcon(rideTurnIcon, -1);
    } else if (trip.active) {
        formatDistance(text, sizeof(text), trip.remainingMetres);
        setIfChanged(rideLeft, text);
        if (trip.simulating) strlcpy(text,"CHẠY THỬ",sizeof(text));
        else if (trip.etaMinute < 1440) snprintf(text, sizeof(text), "Còn lại · đến %02u:%02u", unsigned(trip.etaMinute / 60), unsigned(trip.etaMinute % 60));
        else strlcpy(text, "Còn lại", sizeof(text));
        setIfChanged(rideLeftTitle, text);
        if (trip.maneuver == 6 && trip.turnMetres <= 30) strlcpy(text, "Đã đến nơi", sizeof(text));
        else formatDistance(text, sizeof(text), trip.turnMetres);
        setIfChanged(rideTurn, text);
        setFontSafeText(rideStreet, trip.instruction, true);
        static const char* icons[] = {LV_SYMBOL_UP, LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_REFRESH, LV_SYMBOL_GPS};
        (void)icons; setTurnIcon(rideTurnIcon, trip.maneuver < TURN_KINDS ? trip.maneuver : 0);
    } else {
        setIfChanged(rideLeft, "--");
        setIfChanged(rideLeftTitle, "Còn lại");
        setIfChanged(rideTurn, "Đang dẫn đường");
        if (hasGoogleGuide) setFontSafeText(rideStreet, googleGuide.message[0] ? googleGuide.message : googleGuide.title, true);
        else setIfChanged(rideStreet, "Xem chi tiết ở trang Bản đồ");
        setIfChanged(rideArrowIcon, LV_SYMBOL_GPS); setTurnIcon(rideTurnIcon, -1);
    }
}

// Day/night backlight from the phone's sunrise/sunset (fallback 06:00/18:00),
// ramped over 40 minutes so the change is never sudden.
uint8_t autoBrightnessLevel() {
    const uint8_t manual = hudHardwareSettingsGet().brightness;
    if (!hudAutoBrightness() || !clockValid) return manual;
    WeatherInfo w{};
    int rise = 6 * 60, set = 18 * 60;
    if (navStreamWeather(w) && w.sunriseMinute < 1440 && w.sunsetMinute < 1440 && w.sunriseMinute < w.sunsetMinute) {
        rise = w.sunriseMinute; set = w.sunsetMinute;
    }
    const time_t local = time(nullptr) + utcOffset; tm d{}; gmtime_r(&local, &d);
    const float minute = d.tm_hour * 60 + d.tm_min + d.tm_sec / 60.0f;
    constexpr float DAY = 80, NIGHT = 22, RAMP = 40;
    auto ramp = [&](float centre) { return std::max(0.0f, std::min(1.0f, (minute - (centre - RAMP / 2)) / RAMP)); };
    const float day = ramp(rise) * (1 - ramp(set)); // 0 night .. 1 day
    return uint8_t(NIGHT + (DAY - NIGHT) * day + 0.5f);
}

lv_obj_t* shade(lv_obj_t* parent, int y, int height, lv_opa_t top, lv_opa_t bottom) {
    auto obj = box(parent, 0, y, 320, height, 0x000000);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_opa(obj, top, 0);
    lv_obj_set_style_bg_grad_opa(obj, bottom, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t* igRing(lv_obj_t* parent, int x, uint32_t color, const char* caption, int index) {
    auto arc = lv_arc_create(parent);
    lv_obj_set_pos(arc, x, 148); lv_obj_set_size(arc, 70, 70);
    lv_arc_set_bg_angles(arc, 0, 360); lv_arc_set_rotation(arc, 270);
    lv_arc_set_range(arc, 0, 100); lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, 60, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    igRingValues[index] = label(arc, 0, 18, 70, "--", 0xFFFFFF, textFont);
    lv_obj_set_style_text_align(igRingValues[index], LV_TEXT_ALIGN_CENTER, 0);
    auto cap = label(arc, 0, 40, 70, caption, color, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);
    return arc;
}

// Infograph: big time and date on top, one wide complication, four ring gauges.
void buildInfograph() {
    igFace = lv_obj_create(wallFace);
    lv_obj_remove_style_all(igFace);
    lv_obj_set_size(igFace, 320, 240);
    lv_obj_remove_flag(igFace, LV_OBJ_FLAG_CLICKABLE);
    igTime = label(igFace, 12, 4, 190, "--:--", 0xFFFFFF, &lv_font_montserrat_48);
    igDay = label(igFace, 170, 24, 140, "", 0xFF9F0A, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(igDay, LV_TEXT_ALIGN_RIGHT, 0);
    igDate = label(igFace, 170, 40, 140, "", 0xFFFFFF, textFont);
    lv_obj_set_style_text_align(igDate, LV_TEXT_ALIGN_RIGHT, 0);
    igCard = box(igFace, 10, 66, 300, 74, 0x1C1C1E);
    lv_obj_set_style_radius(igCard, 16, 0);
    lv_obj_remove_flag(igCard, LV_OBJ_FLAG_CLICKABLE);
    igCardTitle = label(igCard, 14, 6, 272, "THỜI TIẾT", 0x64D2FF, &hud_font_vietnamese_14);
    igCardValue = label(igCard, 14, 24, 272, "--", 0xFFFFFF, &hud_font_vietnamese_24);
    lv_label_set_long_mode(igCardValue, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(igCardValue, 28);
    igCardSub = label(igCard, 14, 52, 272, "", 0x98989D, &hud_font_vietnamese_14);
    const uint32_t colors[] = {0x30D158, 0xFF9F0A, 0x64D2FF, 0xFF453A};
    const char* captions[] = {"PIN", "KM", "ÂM", "TIN"};
    for (int i = 0; i < 4; ++i) igRings[i] = igRing(igFace, 10 + i * 78, colors[i], captions[i], i);
    lv_obj_set_hidden(igFace, true);
}

// Full-screen faces follow the theme on a colour background (light themes stay light)
// and use white-on-dark over a photo.
void applyFullFaceColors(bool photo) {
    const bool light = !photo && hudSettingsGet().theme >= 4;
    const uint32_t ink = photo ? 0xFFFFFF : WHITE, soft = photo ? 0xD8D8DC : MUTED;
    for (auto obj : {igTime, igDate, igCardValue, igRingValues[0], igRingValues[1], igRingValues[2], igRingValues[3], wallDate})
        textColorIfChanged(obj, ink);
    textColorIfChanged(igCardSub, soft);
    bgColorIfChanged(igCard, photo ? 0xFFFFFF : light ? PANEL : 0x1C1C1E);
    lv_obj_set_style_bg_opa(igCard, photo ? 80 : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(igCard, light ? 1 : 0, 0);
    lv_obj_set_style_border_color(igCard, lv_color_hex(BUTTON), 0);
    // iPhone face: a white clock would vanish on a light background.
    const uint32_t clock = wallpaperColor();
    textColorIfChanged(wallClock, light && clock == 0xFFFFFF ? WHITE : clock);
    for (int i = 0; i < 3; ++i) {
        bgColorIfChanged(wallPills[i], light ? BUTTON : 0xFFFFFF);
        lv_obj_set_style_bg_opa(wallPills[i], light ? LV_OPA_COVER : 46, 0);
        lv_obj_set_style_border_opa(wallPills[i], light ? 0 : 70, 0);
        textColorIfChanged(wallPillValues[i], ink);
        textColorIfChanged(lv_obj_get_child(wallPills[i], 0), soft);
    }
    // Backdrop: theme background (dark twin over photos is handled by the photo itself).
    const bool infograph = wallState == 3;
    bgColorIfChanged(wallFace, light ? BG : infograph ? 0x000000 : BG);
    lv_obj_set_style_bg_grad_color(wallFace, lv_color_hex(light ? PANEL : infograph ? 0x000000 : 0x05070C), 0);
}

void arcIfChanged(lv_obj_t* arc, int value) {
    value = std::max(0, std::min(100, value));
    if (lv_arc_get_value(arc) != value) lv_arc_set_value(arc, value);
}

void updateInfograph() {
    char text[64]; bool rainy = false;
    WeatherInfo w{};
    if (navStreamWeather(w)) {
        weatherText(text, sizeof(text), rainy);
        setIfChanged(igCardValue, text);
        textColorIfChanged(igCardValue, rainy ? 0xF5A623 : (wallpaperBackground() && wallpaperHas() ? 0xFFFFFF : WHITE));
        if (w.sunriseMinute < 1440 && w.sunsetMinute < 1440)
            snprintf(text, sizeof(text), "Mọc %02u:%02u  ·  Lặn %02u:%02u", unsigned(w.sunriseMinute / 60), unsigned(w.sunriseMinute % 60),
                     unsigned(w.sunsetMinute / 60), unsigned(w.sunsetMinute % 60));
        else text[0] = 0;
        setIfChanged(igCardSub, text);
    } else {
        setIfChanged(igCardValue, "Chưa có dữ liệu");
        setIfChanged(igCardSub, "Mở HUD Connect để cập nhật");
    }
    const BatteryStatus bat = batteryRead();
    if (bat.percent >= 0) snprintf(text, sizeof(text), "%d", bat.percent); else strlcpy(text, "--", sizeof(text));
    setIfChanged(igRingValues[0], text);
    arcIfChanged(igRings[0], std::max(0, bat.percent));
    TodayDistance today{};
    uint32_t metres = 0;
    if (todayText(text, sizeof(text)) && navStreamToday(today)) metres = today.metres;
    if (metres >= 10000) snprintf(text, sizeof(text), "%u", unsigned((metres + 500) / 1000));
    else snprintf(text, sizeof(text), "%u,%u", unsigned(metres / 1000), unsigned(metres % 1000 / 100));
    setIfChanged(igRingValues[1], text);
    arcIfChanged(igRings[1], int(metres / 10 / KM_GOAL));
    snprintf(text, sizeof(text), "%u", unsigned(count));
    setIfChanged(igRingValues[3], text);
    arcIfChanged(igRings[3], int(std::min<size_t>(count, 10) * 10));
}

void buildWallFace() {
    wallFace = box(root, 0, 0, 320, 240, 0x16264A);
    lv_obj_set_style_radius(wallFace, 0, 0);
    // Default backdrop until a photo arrives: deep blue night gradient.
    lv_obj_set_style_bg_grad_color(wallFace, lv_color_hex(0x05070C), 0);
    lv_obj_set_style_bg_grad_dir(wallFace, LV_GRAD_DIR_VER, 0);
    lv_obj_remove_flag(wallFace, LV_OBJ_FLAG_CLICKABLE);
    wallImage = lv_image_create(wallFace);
    lv_obj_set_pos(wallImage, 0, 0);
    lv_obj_set_hidden(wallImage, true);
    wallDim = shade(wallFace, 0, 240, 110, 110);          // even dim behind classic faces
    wallTopShade = shade(wallFace, 0, 110, 150, 0);       // lock layout: date and status bar
    wallBottomShade = shade(wallFace, 130, 110, 0, 190);  // lock layout: widget row
    wallDate = label(wallFace, 0, 22, 320, "", 0xFFFFFF, textFont);
    lv_obj_set_style_text_align(wallDate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(wallDate, 230, 0);
    wallClock = label(wallFace, 0, 64, 320, "--:--", 0xFFFFFF, &hud_font_clock_80);
    lv_obj_set_style_text_align(wallClock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(wallClock, -2, 0);
    wallHint = label(wallFace, 0, 150, 320, "Chọn ảnh nền trong app HUD Connect", 0xFFFFFF, &hud_font_vietnamese_14);
    lv_obj_set_style_text_align(wallHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(wallHint, 170, 0);
    // Glass widgets, like the iOS lock screen widget row.
    const char* titles[] = {"Thời tiết", "Âm lịch", "Hôm nay"};
    for (int i = 0; i < 3; ++i) {
        auto pill = box(wallFace, 10 + i * 102, 182, 96, 48, 0xFFFFFF);
        wallPills[i] = pill;
        lv_obj_set_style_radius(pill, 16, 0);
        lv_obj_set_style_bg_opa(pill, 46, 0);
        lv_obj_set_style_border_width(pill, 1, 0);
        lv_obj_set_style_border_color(pill, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(pill, 70, 0);
        lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);
        auto title = label(pill, 0, 4, 96, titles[i], 0xFFFFFF, &hud_font_vietnamese_14);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_opa(title, 180, 0);
        wallPillValues[i] = label(pill, 4, 22, 88, "--", 0xFFFFFF, textFont);
        lv_obj_set_style_text_align(wallPillValues[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(wallPillValues[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_height(wallPillValues[i], 22);
    }
    buildInfograph();
    lv_obj_move_to_index(wallFace, 0); // under the status bar and pages
    lv_obj_set_hidden(wallFace, true);
}

void setIfChanged(lv_obj_t* obj, const char* text) {
    if (strcmp(lv_label_get_text(obj), text)) lv_label_set_text(obj, text);
}

// Shading is baked into a copy of the photo (uniform dim behind classic faces, top and
// bottom fades for the iPhone face) so each frame is a plain image copy instead of
// alpha-blending full-screen overlays, which cost >100 ms per frame.
const uint16_t* bakeWallpaper(int layout) {
    static uint16_t* shaded = static_cast<uint16_t*>(heap_caps_malloc(WALL_W * WALL_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    const uint16_t* src = wallpaperPixels();
    if (!shaded || !src) return src;
    for (int y = 0; y < WALL_H; ++y) {
        int alpha = 110;                                   // classic faces: ~43% dim
        if (layout == 2) alpha = y < 110 ? 150 * (110 - y) / 110 : y >= 130 ? 190 * (y - 130) / 110 : 0;
        const uint32_t keep = 256 - alpha * 256 / 255;
        const uint16_t* in = src + y * WALL_W;
        uint16_t* out = shaded + y * WALL_W;
        for (int x = 0; x < WALL_W; ++x) {
            const uint32_t p = in[x];
            out[x] = uint16_t((((p >> 11) * keep >> 8) << 11) | ((((p >> 5) & 63) * keep >> 8) << 5) | ((p & 31) * keep >> 8));
        }
    }
    return shaded;
}

void setLockLayout(bool on) {
    for (auto obj : {wallDate, wallClock, wallPills[0], wallPills[1], wallPills[2]}) lv_obj_set_hidden(obj, !on);
    for (auto obj : {wallDim, wallTopShade, wallBottomShade}) lv_obj_set_hidden(obj, true); // baked instead
    lv_obj_set_hidden(pageHeading, on);
    lv_obj_set_hidden(smallClock, on);
}

void updateWallFace(uint32_t now) {
    size_t wallBytes = 0;
    static uint8_t* incoming = static_cast<uint8_t*>(heap_caps_malloc(48 * 1024, MALLOC_CAP_SPIRAM));
    if (incoming && navStreamTakeWallpaper(incoming, 48 * 1024, wallBytes) && wallpaperSave(incoming, wallBytes))
        wallpaperSetBackground(true); // a new photo means "use it"
    // The theme palette depends on the background (dark twin over photos).
    static bool appliedPhoto = false;
    if (appliedPhoto != wallpaperBackground()) { appliedPhoto = wallpaperBackground(); applySettings(); }
    // Visibility follows page swipes immediately; widget text refreshes 4x/s.
    const bool photo = wallpaperBackground();
    const uint8_t face = hudSettingsGet().face;
    const bool full = currentPage == 0 && !rideVisible && (face == PHONE_FACE || face == INFO_FACE);
    const bool lock = full && face == PHONE_FACE;
    const bool active = full || (photo && currentPage == 0);
    // 0 off, 1 photo behind a classic face, 2 iPhone lock screen, 3 Infograph
    const int state = !active ? 0 : lock ? 2 : full ? 3 : 1;
    if (state != wallState) {
        wallState = state;
        lv_obj_set_hidden(wallFace, !active);
        lv_obj_set_style_bg_opa(pages[0], active ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        // Ride face: see-through panel and a glass card over the photo.
        lv_obj_set_style_bg_opa(rideFace, active ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(rideCard, active ? 70 : LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(rideCard, active ? 1 : 0, 0);
        lv_obj_set_style_border_color(rideCard, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(rideCard, 80, 0);
        // Analog dials sit on a translucent disc instead of a solid one.
        lv_obj_set_style_bg_opa(analog, active ? 120 : LV_OPA_COVER, 0);
        setLockLayout(state == 2);
        lv_obj_set_hidden(igFace, state != 3);
        lv_obj_set_hidden(pageHeading, state >= 2);
        lv_obj_set_hidden(smallClock, state >= 2);

        wallShownRevision = 0; // refresh image/colour below
        lv_obj_set_style_bg_color(wallFace, lv_color_hex(BG), 0); // colour background: theme (dark twin)
    }
    const bool active2 = active;
    (void)active2;
    if (!active) return;
    static uint32_t coloredRevision = 0;
    if (coloredRevision != hudSettingsRevision()) { coloredRevision = hudSettingsRevision(); wallShownRevision = 0; }
    static uint32_t last = 0;
    if (now - last < 250 && wallShownRevision == wallpaperRevision()) return;
    last = now;
    if (wallShownRevision != wallpaperRevision()) {
        wallShownRevision = wallpaperRevision();
        const bool has = wallpaperHas();
        if (has) {
            lv_image_cache_drop(&wallDsc);
            wallDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
            wallDsc.header.cf = LV_COLOR_FORMAT_RGB565;
            wallDsc.header.w = WALL_W; wallDsc.header.h = WALL_H;
            wallDsc.header.stride = WALL_W * 2;
            wallDsc.data = reinterpret_cast<const uint8_t*>(bakeWallpaper(wallState));
            wallDsc.data_size = WALL_W * WALL_H * 2;
            lv_image_set_src(wallImage, &wallDsc);
            lv_obj_invalidate(wallImage);
        }
        lv_obj_set_hidden(wallImage, !has || !photo);
        lv_obj_set_hidden(wallHint, has || !photo || wallState != 2);
        applyFullFaceColors(has && photo);

    }
    if (wallState == 3) { updateInfograph(); return; }
    char text[48]; bool rainy = false;
    WeatherInfo w{};
    if (navStreamWeather(w)) {
        weatherText(text, sizeof(text), rainy);
        if (rainy) snprintf(text, sizeof(text), "Mưa %u%%", unsigned(w.rainChance));
        else snprintf(text, sizeof(text), "%d°", (w.tempTenths + (w.tempTenths >= 0 ? 5 : -5)) / 10);
    } else strlcpy(text, "--", sizeof(text));
    setIfChanged(wallPillValues[0], text);
    textColorIfChanged(wallPillValues[0], rainy ? 0xFFC861 : 0xFFFFFF);
    todayText(text, sizeof(text));
    setIfChanged(wallPillValues[2], text);
}

void updateBatteryLabel() {
    static int lastKey=-7;
    const BatteryStatus b=batteryRead();
    const int flags=(b.chargingEstimated?2:0)+(b.usbHost?1:0);
    const int key=b.present ? b.percent*4+flags : b.usbHost ? -6 : -5;
    // Pending readings use keys -4 through -1.
    const bool ready=b.percent>=0;
    if(key==lastKey || !batteryLabel) return;
    lastKey=key;
    const char* icon=b.chargingEstimated ? LV_SYMBOL_CHARGE : b.usbHost ? LV_SYMBOL_USB : "";
    if(ready) lv_label_set_text_fmt(batteryLabel,"%s%d%%",icon,b.percent);
    else if(b.present) lv_label_set_text_fmt(batteryLabel,"%s--%%",icon);
    else lv_label_set_text(batteryLabel,icon);
    lv_obj_set_style_text_color(batteryLabel,lv_color_hex(b.chargingEstimated ? 0x69E4AA : ready && b.percent<=15 ? 0xFF5A79 : WHITE),0);
}

void companionUiTick() {
    const uint32_t now = millis();
    updateBatteryLabel();
    applyPendingQuickSettings();
    static uint32_t qsRevision=0;
    if(qsPanel && !lv_obj_is_hidden(qsPanel) && qsRevision!=hudSettingsRevision()) { qsRevision=hudSettingsRevision(); refreshQuickSettings(); }
    applyPendingAiGesture();
    AiToolRequest tool{};
    if(aiVoicePollTool(tool)) executeAiTool(tool);
    aiVoiceToolTick();
    static AiVoiceView aiView{};
    aiVoiceRead(aiView);
    aiRecordingNow=aiView.recording;
    if(strncmp(heardQuestion,aiView.question,sizeof(heardQuestion)-1)!=0) {
        // Bounded copy that never splits a UTF-8 sequence.
        size_t n=strnlen(aiView.question,sizeof(heardQuestion)-1);
        while(n && n<strlen(aiView.question) && (uint8_t(aiView.question[n])&0xC0)==0x80) --n;
        memcpy(heardQuestion,aiView.question,n); heardQuestion[n]=0;
        if(mapSearchPanel && !lv_obj_is_hidden(mapSearchPanel)) mapSearchRevision=UINT32_MAX;
    }
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
    const int decodeState=mapDecodeState.load(std::memory_order_acquire);
    if (decodeState==MAP_DONE && !mapDragging && !mapPinch.active && !mapPinch.ended) {
        mapDecodeState.store(MAP_IDLE,std::memory_order_relaxed);
        if(mapDecodeMs>=150) Serial.printf("Map decode: %ums\n",unsigned(mapDecodeMs));
        if (!mapLive && currentPage!=AI_PAGE) showPage(2);
        lv_image_cache_drop(&mapFrame);
        const int targetOffset=mapDecodeMotion.offset(now-mapDecodeMotion.startedAt);
        const bool canAlign=mapLive && currentPage==2 && !mapPanMode && !mapPreviewActive
            && mapVisibleMotion.speed && mapDecodeMotion.previousFrame && mapDecodeMotion.previousFrame==mapVisibleMotion.frame
            && !lv_obj_is_visible(mapSearchPanel) && !lv_obj_is_visible(mapStopDialog);
        mapScrollCorrection=0;
        if (canAlign) {
            const int margin=mapDecodeMotion.marginY;
            const int matchedOffset=constrain(mapScrollOffset-mapDecodeMotion.advance,-margin,margin);
            mapScrollCorrection=matchedOffset-targetOffset;
            ++mapMotionAligned;
        }
        mapScrollCorrectionAt=now;
        mapScrollOffset=targetOffset+mapScrollCorrection;
        mapLastShiftAt=now;
        mapScrollAtLimit=false;
        mapVisibleMotion=mapDecodeMotion;
        mapPixelShown=mapPixelTarget;
        mapFrame = {};
        mapFrame.header.magic = LV_IMAGE_HEADER_MAGIC;
        mapFrame.header.cf = LV_COLOR_FORMAT_RGB565;
        mapFrame.header.w = mapVisibleMotion.width();
        mapFrame.header.h = mapVisibleMotion.height();
        mapFrame.header.stride = mapFrame.header.w*2;
        mapFrame.data = reinterpret_cast<const uint8_t*>(mapPixels[mapPixelShown]);
        mapFrame.data_size = mapFrame.header.stride*mapFrame.header.h;
        lv_image_set_src(mapImage, &mapFrame);
        lv_obj_set_size(mapImage,mapFrame.header.w,mapFrame.header.h);
        resetMapPreview();
        lv_obj_set_pos(mapImage,-mapVisibleMotion.marginX,(mapPanMode ? 0 : mapScrollOffset)-mapVisibleMotion.marginY);
        if (mapVisibleMotion.speed) {
            ++mapMotionFrames;
            const uint32_t ink=mapVisibleMotion.light ? 0x1778F2 : 0xFFFFFF;
            bgColorIfChanged(mapVehicleDot,ink);
            textColorIfChanged(mapVehicleArrow,mapVisibleMotion.light ? MAP_ARROW_ON_LIGHT : MAP_ARROW_ON_DARK);
        }
        mapDragFrame=0;
        navStreamMarkDisplayed(mapDecodeFrame);
        lv_obj_set_hidden(mapImage, false);
        lv_obj_set_hidden(mapControls, false);
        mapLive = true;
        mapTripActive = true;
        lv_obj_set_hidden(mapSource,true); lv_obj_set_hidden(lv_obj_get_parent(mapTitle),true);
        lv_obj_set_hidden(mapPortraitHint,viewHeight<=viewWidth);
        mapReceivedAt = now;
    } else if (decodeState==MAP_FAILED) {
        mapDecodeState.store(MAP_IDLE,std::memory_order_relaxed);
    }
    // Take a new JPEG only while the worker is idle: it still reads the old one.
    if (mapDecodeState.load(std::memory_order_acquire)==MAP_IDLE) {
        const uint8_t* jpeg = nullptr;
        size_t jpegSize = 0;
        if (navStreamTake(jpeg, jpegSize, mapDecodeFrame, mapDecodeMotion) && !jpeg) { mapTripActive=false; clearMaps(true); }
        else if (jpegSize && jpeg) startMapDecode(jpeg,jpegSize);
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
    updateRideFace(now);
    updateLock(now);
    static uint32_t statusAt=0;
    if(now-statusAt>=1000) {
        statusAt=now; char line[32]; statusText(line,sizeof(line));
        if(qsStatus && !lv_obj_is_hidden(qsPanel)) setIfChanged(qsStatus,line);
        if(infoStatus && !lv_obj_is_hidden(infoPanel)) setIfChanged(infoStatus,line);
    }
    updateInfoPanel(now);
    updateWallFace(now);
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
    // Hands move every second, so only touch them while the dial is on screen.
    if (currentPage == 0 && !lv_obj_is_hidden(analog)) {
        const float angles[] = {(date.tm_hour % 12 + date.tm_min / 60.0f) * 3.14159265f / 6,
            (date.tm_min + date.tm_sec / 60.0f) * 3.14159265f / 30, date.tm_sec * 3.14159265f / 30};
        const int lengths[] = {35, 52, 58};
        for (int i = 0; i < 3; ++i) {
            handPoints[i][0] = {72, 72};
            handPoints[i][1] = {72 + int(lengths[i] * sinf(angles[i])), 72 - int(lengths[i] * cosf(angles[i]))};
            lv_line_set_points(hands[i], handPoints[i], 2);
        }
    }
    // Labels change once a minute; writing them every second forced needless redraws.
    char hm[8], text[96];
    snprintf(hm, sizeof(hm), "%02d:%02d", date.tm_hour, date.tm_min);
    for (auto obj : {clockLabel, smallClock, rideClock, wallClock, igTime}) setIfChanged(obj, hm);
    const char* days[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};
    snprintf(text, sizeof(text), "%s, %02d/%02d/%04d", days[date.tm_wday],
             date.tm_mday, date.tm_mon + 1, date.tm_year + 1900);
    setIfChanged(dateLabel, text);
    snprintf(text, sizeof(text), "%s, %02d/%02d", days[date.tm_wday], date.tm_mday, date.tm_mon + 1);
    setIfChanged(rideDate, text);
    snprintf(text, sizeof(text), "%s, %d tháng %d", days[date.tm_wday], date.tm_mday, date.tm_mon + 1);
    setIfChanged(wallDate, text);
    static const char* const upperDays[] = {"CHỦ NHẬT", "THỨ HAI", "THỨ BA", "THỨ TƯ", "THỨ NĂM", "THỨ SÁU", "THỨ BẢY"};
    setIfChanged(igDay, upperDays[date.tm_wday]);
    snprintf(text, sizeof(text), "%d/%d", date.tm_mday, date.tm_mon + 1);
    setIfChanged(igDate, text);
    static int lunarDateKey=-1;
    const time_t vietnamTime=utc+7*3600; tm vietnamDate{}; gmtime_r(&vietnamTime,&vietnamDate);
    const int dayKey=(vietnamDate.tm_year+1900)*10000+(vietnamDate.tm_mon+1)*100+vietnamDate.tm_mday;
    if(lunarDateKey!=dayKey) {
        lunarDateKey=dayKey;
        const auto value=lunar::vietnamese(vietnamDate.tm_mday,vietnamDate.tm_mon+1,vietnamDate.tm_year+1900);
        if(value.day) snprintf(lunarText,sizeof(lunarText),"Âm %02d/%02d%s",value.day,value.month,value.leap ? " nhuận" : "");
        if(value.day) lv_label_set_text_fmt(wallPillValues[1],"%d/%d%s",value.day,value.month,value.leap ? " N" : "");
        if(value.day) { lv_label_set_text_fmt(igRingValues[2],"%d/%d",value.day,value.month); lv_arc_set_value(igRings[2],value.day*100/30); }
        else strlcpy(lunarText,"Âm --/--",sizeof(lunarText));
    }
    // Bottom line of the classic faces: lunar date and today's distance.
    char km[16], line[64];
    if(todayText(km,sizeof(km))) snprintf(line,sizeof(line),"%s  ·  Hôm nay %s",lunarText,km);
    else strlcpy(line,lunarText,sizeof(line));
    setIfChanged(lunarLabel,line);
}

void companionUiBluetoothFailed() {
    status(0xFF5A79);
    lv_label_set_text(dateLabel, "Không khởi tạo được Bluetooth");
}

bool companionUiNavigationActive() { return (mapLive && currentPage == 2) || (rideTrip && currentPage == 0); }
uint8_t companionUiBrightness() { return autoBrightnessLevel(); }

bool companionUiConsumeWakeRequest() { const bool r=wakeRequested; wakeRequested=false; return r; }

void companionUiPrintStatus() {
    Serial.printf("Map motion: frames=%u; shifts=%u; aligned=%u; limited=%u; margin=%u; speed_px_tenths=%u\n",
        unsigned(mapMotionFrames),unsigned(mapMotionShifts),unsigned(mapMotionAligned),unsigned(mapMotionLimits),
        unsigned(mapVisibleMotion.marginY),unsigned(mapVisibleMotion.speed));
    { const BatteryStatus b=batteryRead(); Serial.printf("Battery: present=%d; mv=%d; gauge_mv=%d; pct=%d; usb_host=%d; charging_est=%d\n",
        b.present,b.millivolts,b.estimateMillivolts,b.percent,b.usbHost,b.chargingEstimated); }
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

void companionUiJpegBench() {
    static uint8_t* jpeg = static_cast<uint8_t*>(heap_caps_malloc(48 * 1024, MALLOC_CAP_SPIRAM));
    const size_t size = jpeg ? wallpaperReadJpeg(jpeg, 48 * 1024) : 0;
    if (!size || !mapPixels[0]) { Serial.println("JPEGBENCH: no 320x240 JPEG stored"); return; }
    uint32_t total = 0, worst = 0; int ok = 0;
    for (int i = 0; i < 5; ++i) {
        const uint32_t start = micros();
        ok += decodeMapJpeg(jpeg, size, mapPixels[0]);
        const uint32_t us = micros() - start; total += us; worst = std::max(worst, us);
    }
    Serial.printf("JPEGBENCH: bytes=%u ok=%d avg=%ums worst=%ums\n", unsigned(size), ok, unsigned(total / 5000), unsigned(worst / 1000));
}
