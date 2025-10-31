#include "ui.h"
#include <stdio.h>

//------------------------------------------------------------
// Screen Objects
//------------------------------------------------------------
lv_obj_t *ui_SplashScreen;
lv_obj_t *ui_GForceScreen;
lv_obj_t *ui_PeaksScreen;
lv_obj_t *ui_TimerScreen;
lv_obj_t *ui_StampScreen;

//------------------------------------------------------------
// G-Force and Visualizer Objects
//------------------------------------------------------------
lv_obj_t *ui_gforce_dot;
lv_obj_t *ui_bg_image;

//------------------------------------------------------------
// Peak Label Objects
//------------------------------------------------------------
lv_obj_t *ui_peakX_label;
lv_obj_t *ui_peakY_label;
lv_obj_t *ui_peakZ_label;

//------------------------------------------------------------
// Timer and Lap Objects
//------------------------------------------------------------
lv_obj_t *ui_timer_label;
lv_obj_t *ui_reset_button;
lv_obj_t *ui_LapButton;
lv_obj_t *ui_LapLabels[4];
lv_obj_t *ui_TimeLabel1;
lv_obj_t *ui_TimeLabel2;
lv_obj_t *ui_TimerLabel;

//------------------------------------------------------------
// Stamp Trail Container
//------------------------------------------------------------
lv_obj_t *ui_StampContainer;

//------------------------------------------------------------
// Internal Helper
//------------------------------------------------------------
static void ui_event_reset_timer(lv_event_t * e)
{
    lv_label_set_text(ui_timer_label, "0.00");
}

//------------------------------------------------------------
// UI Initialization (stubbed for demo)
//------------------------------------------------------------
void ui_init(void)
{
    printf("LVGL UI: Initializing screens...\r\n");

    //--- Splash Screen ---
    ui_SplashScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_SplashScreen, lv_color_black(), LV_PART_MAIN);
    lv_obj_t *lbl_splash = lv_label_create(ui_SplashScreen);
    lv_label_set_text(lbl_splash, "G-Force Visualizer");
    lv_obj_center(lbl_splash);

    //--- G-Force Screen ---
    ui_GForceScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_GForceScreen, lv_color_hex(0x101010), LV_PART_MAIN);

    // Background image (placeholder)
    ui_bg_image = lv_img_create(ui_GForceScreen);
    lv_img_set_src(ui_bg_image, LV_SYMBOL_DUMMY); // Replace with real image
    lv_obj_center(ui_bg_image);

    // G-Force dot (centered)
    ui_gforce_dot = lv_obj_create(ui_GForceScreen);
    lv_obj_set_size(ui_gforce_dot, 20, 20);
    lv_obj_set_style_bg_color(ui_gforce_dot, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_gforce_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_center(ui_gforce_dot);

    //--- Peaks Screen ---
    ui_PeaksScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_PeaksScreen, lv_color_hex(0x202020), LV_PART_MAIN);
    ui_peakX_label = lv_label_create(ui_PeaksScreen);
    ui_peakY_label = lv_label_create(ui_PeaksScreen);
    ui_peakZ_label = lv_label_create(ui_PeaksScreen);
    lv_label_set_text(ui_peakX_label, "X: 0.00g");
    lv_label_set_text(ui_peakY_label, "Y: 0.00g");
    lv_label_set_text(ui_peakZ_label, "Z: 0.00g");
    lv_obj_align(ui_peakX_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_align(ui_peakY_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_align(ui_peakZ_label, LV_ALIGN_TOP_MID, 0, 50);

    //--- Timer Screen ---
    ui_TimerScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_TimerScreen, lv_color_hex(0x000000), LV_PART_MAIN);

    ui_timer_label = lv_label_create(ui_TimerScreen);
    lv_label_set_text(ui_timer_label, "0.00");
    lv_obj_align(ui_timer_label, LV_ALIGN_CENTER, 0, -20);

    ui_reset_button = lv_btn_create(ui_TimerScreen);
    lv_obj_set_size(ui_reset_button, 80, 40);
    lv_obj_align(ui_reset_button, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_event_cb(ui_reset_button, ui_event_reset_timer, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_reset = lv_label_create(ui_reset_button);
    lv_label_set_text(lbl_reset, "Reset");
    lv_obj_center(lbl_reset);

    //--- Stamp Screen ---
    ui_StampScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_StampScreen, lv_color_hex(0x101010), LV_PART_MAIN);
    ui_StampContainer = lv_obj_create(ui_StampScreen);
    lv_obj_set_size(ui_StampContainer, 240, 240);
    lv_obj_center(ui_StampContainer);
    lv_obj_set_style_bg_opa(ui_StampContainer, LV_OPA_0, LV_PART_MAIN);

    //--- Lap Objects (optional placeholder) ---
    for (int i = 0; i < 4; i++) {
        ui_LapLabels[i] = lv_label_create(ui_TimerScreen);
        char buf[16];
        sprintf(buf, "Lap %d: --.--", i + 1);
        lv_label_set_text(ui_LapLabels[i], buf);
        lv_obj_align(ui_LapLabels[i], LV_ALIGN_BOTTOM_MID, 0, -(i * 20) - 10);
    }

    printf("LVGL UI: Initialization complete.\r\n");
}
