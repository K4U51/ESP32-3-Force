#include "lvgl.h"

// --- Forward declarations ---
float get_gyro_x(void);
float get_gyro_y(void);
float get_current_timer(void);

// --- Images ---
extern lv_img_dsc_t splash_img;
extern lv_img_dsc_t screen1_img;
extern lv_img_dsc_t screen2_img;
extern lv_img_dsc_t screen3_img;
extern lv_img_dsc_t screen4_img;
extern lv_img_dsc_t dot_img;
extern lv_img_dsc_t stamp_img;

// --- Event callback to load next screen ---
void next_screen_event_cb(lv_event_t *e) {
    lv_obj_t *next = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(next);
}

// --- Screen objects ---
lv_obj_t *scr_splash, *scr1, *scr2, *scr3, *scr4;

// --- Screen1: Dot moving with gyro ---
lv_obj_t *dot;

// --- Screen2: Labels for gyro stats ---
lv_obj_t *lbl_peak_y, *lbl_neg_y, *lbl_total_x;

// --- Screen3: Timer labels and lap button ---
lv_obj_t *lbl_times[4];
lv_obj_t *btn_lap;

// --- Screen4: Stamp ---
lv_obj_t *screen4_container;

// --- Global flags to control tasks ---
bool screen1_active = false;
bool screen2_active = false;
bool screen4_active = false;

// --- Initialize screens and UI ---
void init_screens(void) {
    // Create screens
    scr_splash = lv_obj_create(NULL);
    scr1 = lv_obj_create(NULL);
    scr2 = lv_obj_create(NULL);
    scr3 = lv_obj_create(NULL);
    scr4 = lv_obj_create(NULL);

    // Set background images
    lv_obj_set_style_bg_img_src(scr_splash, &splash_img, 0);
    lv_obj_set_style_bg_img_src(scr1, &screen1_img, 0);
    lv_obj_set_style_bg_img_src(scr2, &screen2_img, 0);
    lv_obj_set_style_bg_img_src(scr3, &screen3_img, 0);
    lv_obj_set_style_bg_img_src(scr4, &screen4_img, 0);

    // --- Screen1 dot ---
    dot = lv_img_create(scr1);
    lv_img_set_src(dot, &dot_img);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);

    // --- Screen2 labels ---
    lbl_peak_y = lv_label_create(scr2);
    lv_label_set_text(lbl_peak_y, "Y+: 0.00g");
    lv_obj_align(lbl_peak_y, LV_ALIGN_TOP_LEFT, 10, 10);

    lbl_neg_y = lv_label_create(scr2);
    lv_label_set_text(lbl_neg_y, "Y-: 0.00g");
    lv_obj_align(lbl_neg_y, LV_ALIGN_TOP_LEFT, 10, 40);

    lbl_total_x = lv_label_create(scr2);
    lv_label_set_text(lbl_total_x, "X: 0.00g");
    lv_obj_align(lbl_total_x, LV_ALIGN_TOP_LEFT, 10, 70);

    // --- Screen3 timer + lap button ---
    btn_lap = lv_btn_create(scr3);
    lv_obj_set_size(btn_lap, 100, 50);
    lv_obj_align(btn_lap, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *btn_label = lv_label_create(btn_lap);
    lv_label_set_text(btn_label, "Lap");

    for(int i=0;i<4;i++){
        lbl_times[i] = lv_label_create(scr3);
        lv_label_set_text(lbl_times[i], "--:--.--");
        lv_obj_align(lbl_times[i], LV_ALIGN_TOP_MID, 0, 10 + i*30);
    }

    // --- Screen4 container for stamps ---
    screen4_container = lv_obj_create(scr4);
    lv_obj_set_size(screen4_container, 480, 480); // fill screen
    lv_obj_clear_flag(screen4_container, LV_OBJ_FLAG_SCROLLABLE);

    // --- Touch navigation ---
    lv_obj_add_event_cb(scr_splash, next_screen_event_cb, LV_EVENT_CLICKED, scr1);
    lv_obj_add_event_cb(scr1, next_screen_event_cb, LV_EVENT_CLICKED, scr2);
    lv_obj_add_event_cb(scr2, next_screen_event_cb, LV_EVENT_CLICKED, scr3);
    lv_obj_add_event_cb(scr3, next_screen_event_cb, LV_EVENT_CLICKED, scr4);
    lv_obj_add_event_cb(scr4, next_screen_event_cb, LV_EVENT_CLICKED, scr1); // loop

    // --- Lap button callback ---
    lv_obj_add_event_cb(btn_lap, [](lv_event_t *e){
        static int idx = 0;
        float time = get_current_timer();
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f s", time);
        lv_label_set_text(lbl_times[idx], buf);
        idx = (idx + 1) % 4;
    }, LV_EVENT_CLICKED, NULL);

    // Load splash screen first
    lv_scr_load(scr_splash);
}

// --- Screen tasks ---
// Screen1 dot update
void screen1_dot_task(void *param){
    while(1){
        if(screen1_active){
            int16_t x = (int16_t)(get_gyro_x()*90);
            int16_t y = (int16_t)(get_gyro_y()*90);
            lv_obj_set_pos(dot, 240 + x, 240 + y);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Screen2 labels update
void screen2_label_task(void *param){
    float y_peak=0, y_neg=0, x_total=0;
    while(1){
        if(screen2_active){
            float y = get_gyro_y();
            float x = get_gyro_x();
            if(y > y_peak) y_peak=y;
            if(y < y_neg) y_neg=y;
            x_total += x;
            char buf[32];
            snprintf(buf,sizeof(buf),"Y+: %.2fg", y_peak); lv_label_set_text(lbl_peak_y, buf);
            snprintf(buf,sizeof(buf),"Y-: %.2fg", y_neg); lv_label_set_text(lbl_neg_y, buf);
            snprintf(buf,sizeof(buf),"X: %.2fg", x_total); lv_label_set_text(lbl_total_x, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Screen4 stamp task
void screen4_stamp_task(void *param){
    while(1){
        if(screen4_active){
            int16_t gx = (int16_t)get_gyro_x();
            int16_t gy = (int16_t)get_gyro_y();
            lv_obj_t *stamp = lv_img_create(screen4_container);
            lv_img_set_src(stamp, &stamp_img);
            lv_obj_set_pos(stamp, 240 + gx, 240 + gy);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// --- LVGL screen change hook to update active flags ---
void lv_scr_change_hook(lv_event_t *e){
    lv_obj_t *act = lv_scr_act();
    screen1_active = (act == scr1);
    screen2_active = (act == scr2);
    screen4_active = (act == scr4);
}
