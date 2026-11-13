#pragma once
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// Shared by firmware and the desktop layout renderer; no board dependencies.
struct HudDestinationUi {
    lv_obj_t *panel,*heading,*body,*status,*list,*mic,*start,*cancel;
    lv_obj_t *rows[5],*titles[5],*subtitles[5];
    lv_obj_t* spinner=nullptr;
    static lv_obj_t* searchButton(lv_obj_t* parent,int x,int y,uint32_t background,uint32_t accent,lv_event_cb_t cb) {
        auto obj=lv_button_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,34,32);
        lv_obj_set_style_pad_all(obj,0,0); lv_obj_set_style_bg_color(obj,lv_color_hex(background),0);
        auto lens=surface(obj,7,6,13,13,background);
        lv_obj_set_style_bg_opa(lens,LV_OPA_TRANSP,0); lv_obj_set_style_radius(lens,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_border_width(lens,2,0); lv_obj_set_style_border_color(lens,lv_color_hex(accent),0);
        lv_obj_set_clickable(lens,false);
        auto handle=lv_line_create(obj);
        static const lv_point_precise_t points[]={{18,18},{25,25}};
        lv_line_set_points(handle,points,2); lv_obj_set_style_line_width(handle,2,0);
        lv_obj_set_style_line_rounded(handle,true,0); lv_obj_set_style_line_color(handle,lv_color_hex(accent),0);
        if(cb) lv_obj_add_event_cb(obj,cb,LV_EVENT_CLICKED,nullptr);
        return obj;
    }
    static lv_obj_t* text(lv_obj_t* parent,int x,int y,int width,const char* value,const lv_font_t* font,uint32_t ink) {
        auto obj=lv_label_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_width(obj,width);
        lv_obj_set_style_text_font(obj,font,0); lv_obj_set_style_text_color(obj,lv_color_hex(ink),0);
        lv_label_set_long_mode(obj,LV_LABEL_LONG_MODE_WRAP); lv_label_set_text(obj,value); return obj;
    }
    static lv_obj_t* surface(lv_obj_t* parent,int x,int y,int width,int height,uint32_t color) {
        auto obj=lv_obj_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,width,height);
        lv_obj_set_style_pad_all(obj,0,0); lv_obj_set_style_border_width(obj,0,0);
        lv_obj_set_style_radius(obj,10,0); lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
        lv_obj_set_scrollable(obj,false); return obj;
    }
    static lv_obj_t* button(lv_obj_t* parent,int x,int y,int width,int height,uint32_t color,
                            const char* caption,const lv_font_t* font,uint32_t ink,lv_event_cb_t cb,void* data=nullptr) {
        auto obj=lv_button_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,width,height);
        lv_obj_set_style_pad_all(obj,0,0); lv_obj_set_style_border_width(obj,0,0);
        lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
        auto label=text(obj,0,0,width-8,caption,font,ink);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(label);
        if(cb) lv_obj_add_event_cb(obj,cb,LV_EVENT_CLICKED,data); return obj;
    }
    // Icon on the left of a full-width button; the caption (child 0) stays centred.
    static void leadingIcon(lv_obj_t* button,const char* symbol,uint32_t ink) {
        auto icon=lv_label_create(button); lv_label_set_text(icon,symbol);
        lv_obj_set_style_text_font(icon,&lv_font_montserrat_18,0); lv_obj_set_style_text_color(icon,lv_color_hex(ink),0);
        lv_obj_add_flag(icon,LV_OBJ_FLAG_IGNORE_LAYOUT); lv_obj_align(icon,LV_ALIGN_LEFT_MID,16,0);
    }
    // Round marker at the start of a list row (number or symbol).
    static lv_obj_t* rowBadge(lv_obj_t* row,int rowHeight,const char* text,uint32_t fill,uint32_t ink,const lv_font_t* font) {
        auto badge=surface(row,8,(rowHeight-30)/2,30,30,fill);
        lv_obj_set_style_radius(badge,LV_RADIUS_CIRCLE,0); lv_obj_set_clickable(badge,false);
        auto label=lv_label_create(badge); lv_label_set_text(label,text);
        lv_obj_set_style_text_font(label,font,0); lv_obj_set_style_text_color(label,lv_color_hex(ink),0);
        lv_obj_center(label); return badge;
    }
    static lv_obj_t* stopConfirmation(lv_obj_t* parent,int width,uint32_t background,uint32_t buttonColor,
                    uint32_t ink,const lv_font_t* font,lv_event_cb_t cancel,lv_event_cb_t confirm) {
        auto dialog=surface(parent,8,30,width-16,140,background);
        auto message=text(dialog,12,12,width-40,"Kết thúc chuyến đi hiện tại?",font,ink);
        lv_obj_set_height(message,72); lv_label_set_long_mode(message,LV_LABEL_LONG_MODE_WRAP);
        const int buttonWidth=(width-48)/2;
        button(dialog,10,98,buttonWidth,34,buttonColor,"HỦY",font,ink,cancel);
        button(dialog,22+buttonWidth,98,buttonWidth,34,0xC63838,"KẾT THÚC",font,0xFFFFFF,confirm);
        return dialog;
    }
    void create(lv_obj_t* parent,int width,int height,uint32_t panelColor,uint32_t buttonColor,
                uint32_t accent,uint32_t ink,uint32_t muted,uint32_t inverse,const lv_font_t* normal,
                const lv_font_t* small,lv_event_cb_t onMic,lv_event_cb_t onChoose,lv_event_cb_t onStart,lv_event_cb_t onCancel) {
        panel=surface(parent,0,0,width,height,panelColor);
        heading=text(panel,12,7,width-64,"TÌM ĐIỂM ĐẾN",small,accent);
        cancel=button(panel,width-42,2,36,32,buttonColor,LV_SYMBOL_CLOSE,&lv_font_montserrat_18,accent,onCancel);
        body=surface(panel,8,38,width-16,height-104,panelColor);
        lv_obj_set_scroll_dir(body,LV_DIR_VER);
        status=text(body,4,0,width-28,"Giữ mic rồi nói nơi muốn đến",normal,ink);
        list=surface(panel,8,64,width-16,height-72,panelColor);
        lv_obj_set_scrollable(list,true); lv_obj_set_scroll_dir(list,LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list,6,0);
        lv_obj_set_style_pad_right(list,4,0);
        for(int i=0;i<5;++i) {
            rows[i]=surface(list,0,0,width-20,52,buttonColor);
            lv_obj_set_style_radius(rows[i],12,0);
            lv_obj_set_clickable(rows[i],true);
            char number[4]; snprintf(number,sizeof(number),"%d",i+1);
            rowBadge(rows[i],52,number,accent,inverse,small);
            titles[i]=text(rows[i],46,4,width-72,"",normal,ink);
            subtitles[i]=text(rows[i],46,29,width-72,"",small,muted);
            lv_label_set_long_mode(titles[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(titles[i],23);
            lv_label_set_long_mode(subtitles[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(subtitles[i],18);
            if(onChoose) lv_obj_add_event_cb(rows[i],onChoose,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(i)));
        }
        mic=button(panel,12,height-54,width-24,44,accent,"GIỮ MIC ĐỂ NÓI",small,inverse,nullptr);
        lv_obj_set_style_radius(mic,22,0); leadingIcon(mic,LV_SYMBOL_AUDIO,inverse);
        if(onMic) {
            lv_obj_add_event_cb(mic,onMic,LV_EVENT_PRESSED,nullptr);
            lv_obj_add_event_cb(mic,onMic,LV_EVENT_RELEASED,nullptr);
            lv_obj_add_event_cb(mic,onMic,LV_EVENT_PRESS_LOST,nullptr);
        }
        start=button(panel,12,height-52,width-24,44,0x30D158,"BẮT ĐẦU",normal,0x04170C,onStart);
        lv_obj_set_style_radius(start,22,0); leadingIcon(start,LV_SYMBOL_PLAY,0x04170C);
        // Shown while the phone fetches GPS and plans the route.
        spinner=lv_spinner_create(panel);
        lv_obj_set_size(spinner,44,44); lv_obj_align(spinner,LV_ALIGN_BOTTOM_MID,0,-14);
        lv_obj_set_style_arc_color(spinner,lv_color_hex(accent),LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(spinner,5,LV_PART_INDICATOR); lv_obj_set_style_arc_width(spinner,5,LV_PART_MAIN);
        lv_obj_add_flag(spinner,LV_OBJ_FLAG_HIDDEN);
        show(0,"Giữ mic rồi nói nơi muốn đến",0,nullptr,width,height);
    }
    uint32_t shownSession=0;
    uint8_t shownPhase=255;
    void show(uint8_t phase,const char* message,int count,const char* const* items,int width,int height,uint32_t session=0,
              const char* heard="") {
        const bool results=phase==1 && count>0,preview=phase==2 || phase==5,waiting=phase==6;
        if(shownSession!=session || shownPhase!=phase) {
            lv_obj_scroll_to_y(body,0,LV_ANIM_OFF); lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);
            shownSession=session; shownPhase=phase;
        }
        lv_label_set_text(heading,preview ? "XEM CHUYẾN ĐI" : waiting ? "CHUẨN BỊ TUYẾN" : "TÌM ĐIỂM ĐẾN");
        lv_obj_set_size(body,width-16,results ? 22 : preview ? height-96 : waiting ? height-110 : height-104);
        lv_obj_set_scrollable(body,!results);
        // The bundled Vietnamese font does not include U+2026.
        char readable[512]; size_t used=0;
        for(const char* p=message; *p && used+4<sizeof(readable);) {
            if(strncmp(p,"\xE2\x80\xA6",3)==0) { memcpy(readable+used,"...",3); used+=3; p+=3; }
            else readable[used++]=*p++;
        }
        readable[used]=0;
        // Always show what the microphone understood, so a wrong result is explainable.
        char text[700];
        const bool hasHeard=heard && heard[0] && !preview && !waiting;
        if(results) snprintf(text,sizeof(text),hasHeard ? "Kết quả: \"%s\"" : "Chọn điểm đến",heard);
        else if(hasHeard) snprintf(text,sizeof(text),"Bạn nói: \"%s\"\n%s",heard,readable);
        else snprintf(text,sizeof(text),"%s",readable);
        lv_label_set_text(status,text);
        lv_obj_set_hidden(list,!results); lv_obj_set_hidden(mic,results || preview || waiting);
        lv_obj_set_hidden(start,!preview);
        if(spinner) lv_obj_set_hidden(spinner,!waiting);
        for(int i=0;i<5;++i) {
            const bool visible=results && i<count;
            lv_obj_set_hidden(rows[i],!visible);
            if(!visible || !items) continue;
            char title[192]; const char* split=strchr(items[i],'\n');
            const int length=split ? int(split-items[i]) : int(strlen(items[i]));
            snprintf(title,sizeof(title),"%.*s",length,items[i]);
            lv_label_set_text(titles[i],title);
            lv_label_set_text(subtitles[i],split ? split+1 : "");
        }
    }
};

struct HudMapHomeUi {
    lv_obj_t *panel,*heading,*list,*empty,*mic;
    lv_obj_t *rows[5],*titles[5],*addresses[5];
    uint32_t shownRevision=UINT32_MAX;
    bool shownRecording=false,shownConnected=false;
    void create(lv_obj_t* parent,int width,int height,uint32_t background,uint32_t buttonColor,
                uint32_t accent,uint32_t ink,uint32_t muted,uint32_t inverse,
                const lv_font_t* normal,const lv_font_t* small,lv_event_cb_t onMic,lv_event_cb_t onChoose) {
        panel=HudDestinationUi::surface(parent,0,0,width,height,background);
        lv_obj_set_gesture_bubble(panel,true);
        heading=HudDestinationUi::text(panel,12,6,width-24,"ĐIỂM ĐẾN GẦN ĐÂY",small,accent);
        empty=HudDestinationUi::text(panel,12,44,width-24,"Chưa có lịch sử.\nGiữ nút bên dưới để tìm điểm đến.",normal,muted);
        list=HudDestinationUi::surface(panel,8,31,width-16,height-91,background);
        lv_obj_set_scrollable(list,true); lv_obj_set_scroll_dir(list,LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list,6,0);
        lv_obj_set_style_pad_right(list,4,0); lv_obj_set_gesture_bubble(list,true);
        for(int i=0;i<5;++i) {
            rows[i]=HudDestinationUi::surface(list,0,0,width-20,50,buttonColor);
            lv_obj_set_style_radius(rows[i],12,0);
            lv_obj_set_clickable(rows[i],true); lv_obj_set_gesture_bubble(rows[i],true);
            auto badge=HudDestinationUi::rowBadge(rows[i],50,LV_SYMBOL_GPS,accent,inverse,&lv_font_montserrat_14);
            lv_obj_set_gesture_bubble(badge,true);
            titles[i]=HudDestinationUi::text(rows[i],46,4,width-72,"",normal,ink);
            addresses[i]=HudDestinationUi::text(rows[i],46,28,width-72,"",small,muted);
            lv_label_set_long_mode(titles[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(titles[i],23);
            lv_label_set_long_mode(addresses[i],LV_LABEL_LONG_MODE_DOTS); lv_obj_set_height(addresses[i],18);
            if(onChoose) lv_obj_add_event_cb(rows[i],onChoose,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(i)));
        }
        mic=HudDestinationUi::button(panel,12,height-52,width-24,44,accent,"GIỮ ĐỂ NÓI TÌM ĐIỂM ĐẾN",small,inverse,nullptr);
        lv_obj_set_style_radius(mic,22,0); HudDestinationUi::leadingIcon(mic,LV_SYMBOL_AUDIO,inverse);
        if(onMic) {
            lv_obj_add_event_cb(mic,onMic,LV_EVENT_PRESSED,nullptr);
            lv_obj_add_event_cb(mic,onMic,LV_EVENT_RELEASED,nullptr);
            lv_obj_add_event_cb(mic,onMic,LV_EVENT_PRESS_LOST,nullptr);
        }
    }
    void show(int count,const char* const* items,uint32_t revision,bool connected,bool recording) {
        if(shownRevision==revision && shownConnected==connected && shownRecording==recording) return;
        if(shownRevision!=revision) lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);
        shownRevision=revision; shownConnected=connected; shownRecording=recording;
        lv_label_set_text(heading,connected ? "ĐIỂM ĐẾN GẦN ĐÂY" : "GẦN ĐÂY - CHƯA KẾT NỐI");
        lv_obj_set_hidden(empty,count>0); lv_obj_set_hidden(list,count==0);
        lv_label_set_text(empty,connected ? "Chưa có lịch sử.\nGiữ nút bên dưới để tìm điểm đến." : "Mở HUD Connect để đồng bộ lịch sử và tìm đường.");
        lv_label_set_text(lv_obj_get_child(mic,0),recording ? "ĐANG NGHE - THẢ ĐỂ TÌM" : "GIỮ ĐỂ NÓI TÌM ĐIỂM ĐẾN");
        for(int i=0;i<5;++i) {
            const bool visible=i<count && items && items[i][0]; lv_obj_set_hidden(rows[i],!visible);
            if(!visible) continue;
            char name[192]; const char* split=strchr(items[i],'\n');
            snprintf(name,sizeof(name),"%.*s",split ? int(split-items[i]) : int(strlen(items[i])),items[i]);
            lv_label_set_text(titles[i],name); lv_label_set_text(addresses[i],split ? split+1 : "");
        }
    }
};
