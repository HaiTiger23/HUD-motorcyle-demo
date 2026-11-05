#include <hud_destination_ui.h>
#include <companion_fonts.h>
#include <vector>
#include <fstream>
#include <string>
#include <assert.h>
static std::vector<unsigned char> pixels(320*240*4);
static void flush(lv_display_t* display,const lv_area_t* area,uint8_t* data) {
    for(int y=area->y1;y<=area->y2;++y)
        memcpy(pixels.data()+(y*320+area->x1)*4,data+(y-area->y1)*(area->x2-area->x1+1)*4,(area->x2-area->x1+1)*4);
    lv_display_flush_ready(display);
}
int main(int argc,char** argv) {
    assert(argc==2); lv_init(); auto display=lv_display_create(320,240);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    static uint8_t buffer[320*240*4];
    lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    const char* items[]={"Trống Đồng Palace\n1.2 km · Đường Z115, Thành phố Thái Nguyên",
        "Trống Đồng Palace Hoàng Quốc Việt\n2.4 km · Phường Nghĩa Đô, Hà Nội",
        "Nhà hàng Trống Đồng\n3.8 km · Đại lộ Võ Nguyên Giáp",
        "Trung tâm hội nghị Trống Đồng\n5.6 km · Đường Lê Trọng Tấn",
        "Trống Đồng Riverside\n8.1 km · Đường ven sông"};
    for(int scene=0;scene<17;++scene) {
        const bool light=scene>=11 ? scene%2==0 : scene>=4 && scene!=7 && scene!=9; const int phase=scene==2 ? 6 : scene==3 ? 0 : (scene==0 || scene==1 || scene==4) ? 1 : 2;
        auto root=lv_screen_active(); lv_obj_clean(root);
        lv_obj_set_style_bg_color(root,lv_color_hex(light ? 0xECF6F0 : 0x0B1320),0);
        auto clock=HudDestinationUi::text(root,12,6,260,"HUD CONNECT                       12:34",&hud_font_vietnamese_14,light ? 0x142B21 : 0xE8F3FF);
        (void)clock;
        auto page=HudDestinationUi::surface(root,10,32,300,200,light ? 0xF7FBF8 : 0x17283C);
        HudDestinationUi ui{};
        ui.create(page,300,200,light ? 0xF7FBF8 : 0x17283C,light ? 0xDCECE2 : 0x243B55,
            light ? 0x007A50 : 0x43D9CB,light ? 0x142B21 : 0xF2F7FF,
            light ? 0x506D5C : 0xA7BDCE,light ? 0xFFFFFF : 0x0B1320,
            &hud_font_vietnamese,&hud_font_vietnamese_14,nullptr,nullptr,nullptr,nullptr);
        const char* text=phase==6 ? "Đang lấy vị trí mới và tính tuyến…" : phase==0 ? "Giữ mic rồi nói nơi bạn muốn đến" : (scene==6 || scene==10) ?
            "Trung tâm hội nghị và tiệc cưới Trống Đồng Palace Hoàng Quốc Việt, Phường Nghĩa Đô, Thành phố Hà Nội\n12.6 km · 28 phút" : "Trống Đồng Palace\n1.2 km · 6 phút";
        ui.show(phase,text,5,items,300,200,1);
        if(scene==7 || scene==8) {
            lv_obj_set_hidden(ui.panel,true);
            HudDestinationUi::text(page,12,8,230,"BẢN ĐỒ",&hud_font_vietnamese_14,light ? 0x142B21 : 0xF2F7FF);
            HudDestinationUi::searchButton(page,262,3,light ? 0xDCECE2 : 0x243B55,light ? 0x007A50 : 0x43D9CB,nullptr);
        }
        if(scene==9) {
            lv_obj_set_hidden(ui.panel,true);
            auto dialog=HudDestinationUi::stopConfirmation(page,300,0x17283C,0x243B55,0xF2F7FF,&hud_font_vietnamese,nullptr,nullptr);
            lv_obj_set_style_bg_color(root,lv_color_hex(0x0B1320),0);
            (void)dialog;
        }
        if(scene>=11) {
            lv_obj_set_hidden(ui.panel,true);
            HudMapHomeUi home{};
            home.create(page,300,200,light ? 0xF7FBF8 : 0x17283C,light ? 0xDCECE2 : 0x243B55,
                light ? 0x007A50 : 0x43D9CB,light ? 0x142B21 : 0xF2F7FF,
                light ? 0x506D5C : 0xA7BDCE,light ? 0xFFFFFF : 0x0B1320,
                &hud_font_vietnamese,&hud_font_vietnamese_14,nullptr,nullptr);
            const char* history[]={"Trống Đồng Palace\nĐường Z115, Thành phố Thái Nguyên",
                "Nhà hát Lớn Hà Nội\nSố 1 Tràng Tiền, Hà Nội","Hồ Hoàn Kiếm\nPhường Hoàn Kiếm, Hà Nội",
                "Nhà hàng Trống Đồng\nĐường Lê Trọng Tấn","Bến xe Mỹ Đình\nĐường Phạm Hùng, Hà Nội"};
            home.show(scene==13 || scene==14 ? 0 : 5,history,1,scene!=14,scene==15);
            lv_obj_update_layout(root);
            lv_area_t listBounds{},micBounds{};
            lv_obj_get_coords(home.list,&listBounds); lv_obj_get_coords(home.mic,&micBounds);
            assert(listBounds.y2<micBounds.y1); assert(micBounds.y2<232);
            if(scene==16) lv_obj_scroll_to_y(home.list,LV_COORD_MAX,LV_ANIM_OFF);
        }
        lv_obj_update_layout(root);
        if(scene==1) lv_obj_scroll_to_y(ui.list,LV_COORD_MAX,LV_ANIM_OFF);
        if(scene==10) lv_obj_scroll_to_y(ui.body,LV_COORD_MAX,LV_ANIM_OFF);
        for(int i=0;i<5;++i) {lv_tick_inc(20);lv_timer_handler();}
        lv_area_t footer{},body{}; lv_obj_get_coords(ui.start,&footer);lv_obj_get_coords(ui.body,&body);
        if(phase==2) { assert(body.y2<footer.y1); assert(footer.y2<232); }
        if(phase==1) { assert(lv_obj_get_scroll_bottom(ui.list)>0 || scene==1); }
        std::ofstream file(std::string(argv[1])+"/scene-"+std::to_string(scene)+".ppm",std::ios::binary);
        file<<"P6\n320 240\n255\n";
        for(int i=0;i<320*240;++i) {const char rgb[]={char(pixels[i*4+2]),char(pixels[i*4+1]),char(pixels[i*4])};file.write(rgb,3);}
    }
    puts("Rendered 17 scenes with actual LVGL and firmware fonts; preview footer bounds and list scrolling passed.");
}
