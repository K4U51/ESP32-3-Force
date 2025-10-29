/* 
  GForce Gauge ESP32-S3 LVGL Example
  -----------------------------------
  Features:
  - 4-screen LVGL interface with touch navigation.
  - Screen 1: Moving dot visualized from gyro values.
  - Screen 2: Peak/negative/total value tracking.
  - Screen 3: Timer with Lap + Reset buttons.
  - Screen 4: "Stamp" trail of gyro points.
  
  Setup Notes:
  1. Ensure your lvgl display/touch drivers are initialized before init_screens().
  2. Implement:
       float get_gyro_x();  // return X-axis G-force
       float get_gyro_y();  // return Y-axis G-force
  3. Add your images as lv_img_dsc_t in .c files (e.g. dot_img.c) and declare them below.
*/

#include <Arduino.h>
#include "lvgl.h"

// --- Images (from SquareLine assets) ---
extern lv_img_dsc_t splash_img;
extern lv_img_dsc_t screen1_img;
extern lv_img_dsc_t screen2_img;
extern lv_img_dsc_t screen3_img;
extern lv_img_dsc_t screen4_img;
extern lv_img_dsc_t dot_img;
//extern lv_img_dsc_t stamp_img;

// --- Forward declarations for user functions ---
float get_gyro_x(void);
float get_gyro_y(void);

// --- Screen objects ---
lv_obj_t *scr_splash, *scr1, *scr2, *scr3, *scr4;
lv_obj_t *dot;
lv_obj_t *lbl_peak_y, *lbl_neg_y, *lbl_total_x;
lv_obj_t *lbl_timer, *btn_lap, *btn_reset;
lv_obj_t *lbl_times[4];
lv_obj_t *screen4_container;

// --- Timer data ---
float timer_value = 0;
float lap_times[4] = {0};
int lap_idx = 0;
bool timer_running = false;

// --- Screen activity flags ---
bool screen1_active = false;
bool screen2_active = false;
bool screen3_active = false;
bool screen4_active = false;

// --- Display constants ---
#define SCREEN_CENTER_X 240
#define SCREEN_CENTER_Y 240
#define DOT_RADIUS      10   // half image width (adjust if needed)

// --- Touch navigation ---
void next_screen_event_cb(lv_event_t *e) {
    lv_obj_t *next = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(next);
}

// --- Screen change hook ---
void lv_scr_change_hook(lv_event_t *e) {
    lv_obj_t *act = lv_scr_act();
    screen1_active = (act == scr1);
    screen2_active = (act == scr2);
    screen3_active = (act == scr3);
    screen4_active = (act == scr4);
    timer_running = (act == scr3); // timer runs only on Screen3
}

// --- Screen3 lap button ---
void lap_button_cb(lv_event_t *e) {
    if (!screen3_active) return;

    lap_times[lap_idx] = timer_value;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f s", lap_times[lap_idx]);
    lv_label_set_text(lbl_times[lap_idx], buf);
    lap_idx = (lap_idx + 1) % 4;

    lv_event_set_ready(e); // Stop propagation
}

// --- Screen3 reset button ---
void reset_button_cb(lv_event_t *e) {
    timer_value = 0;
    lap_idx = 0;
    for (int i = 0; i < 4; i++) {
        lap_times[i] = 0;
        lv_label_set_text(lbl_times[i], "--:--.--");
    }
    lv_label_set_text(lbl_timer, "0.00 s");
    lv_event_set_ready(e);
}

// --- Initialize screens ---
void init_screens(void) {
    // Create screens
    scr_splash = lv_obj_create(NULL);
    scr1 = lv_obj_create(NULL);
    scr2 = lv_obj_create(NULL);
    scr3 = lv_obj_create(NULL);
    scr4 = lv_obj_create(NULL);

    // Background images
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

    // --- Screen3 timer + buttons ---
    lbl_timer = lv_label_create(scr3);
    lv_label_set_text(lbl_timer, "0.00 s");
    lv_obj_align(lbl_timer, LV_ALIGN_TOP_MID, 0, 10);

    btn_lap = lv_btn_create(scr3);
    lv_obj_set_size(btn_lap, 100, 50);
    lv_obj_align(btn_lap, LV_ALIGN_BOTTOM_LEFT, 30, -10);
    lv_obj_t *btn_label = lv_label_create(btn_lap);
    lv_label_set_text(btn_label, "Lap");
    lv_obj_add_event_cb(btn_lap, lap_button_cb, LV_EVENT_CLICKED, NULL);

    btn_reset = lv_btn_create(scr3);
    lv_obj_set_size(btn_reset, 100, 50);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_RIGHT, -30, -10);
    lv_obj_t *btn_reset_label = lv_label_create(btn_reset);
    lv_label_set_text(btn_reset_label, "Reset");
    lv_obj_add_event_cb(btn_reset, reset_button_cb, LV_EVENT_CLICKED, NULL);

    for (int i = 0; i < 4; i++) {
        lbl_times[i] = lv_label_create(scr3);
        lv_label_set_text(lbl_times[i], "--:--.--");
        lv_obj_align(lbl_times[i], LV_ALIGN_TOP_MID, 0, 40 + i * 30);
    }

    // --- Screen4 stamp container ---
    screen4_container = lv_obj_create(scr4);
    lv_obj_set_size(screen4_container, 480, 480);
    lv_obj_clear_flag(screen4_container, LV_OBJ_FLAG_SCROLLABLE);

    // --- Touch navigation ---
    lv_obj_add_event_cb(scr_splash, next_screen_event_cb, LV_EVENT_CLICKED, scr1);
    lv_obj_add_event_cb(scr1, next_screen_event_cb, LV_EVENT_CLICKED, scr2);
    lv_obj_add_event_cb(scr2, next_screen_event_cb, LV_EVENT_CLICKED, scr3);
    lv_obj_add_event_cb(scr3, next_screen_event_cb, LV_EVENT_CLICKED, scr4);
    lv_obj_add_event_cb(scr4, next_screen_event_cb, LV_EVENT_CLICKED, scr1);

    // Load splash screen
    lv_scr_load(scr_splash);

    // Attach screen change hook
    lv_obj_add_event_cb(lv_scr_act(), lv_scr_change_hook, LV_EVENT_SCREEN_CHANGED, NULL);
}

// --- Screen1: moving dot ---
void screen1_dot_task(void *param) {
    static float filtered_x = 0, filtered_y = 0;
    const float alpha = 0.15f;

    while (1) {
        if (screen1_active) {
            float gx = get_gyro_x();
            float gy = get_gyro_y();

            // Smooth the values (low-pass filter)
            filtered_x = filtered_x * (1 - alpha) + gx * alpha;
            filtered_y = filtered_y * (1 - alpha) + gy * alpha;

            // Scale for display movement
            int16_t x = (int16_t)(filtered_x * 100);
            int16_t y = (int16_t)(filtered_y * 100);

            // Center around screen midpoint
            lv_obj_set_pos(dot, SCREEN_CENTER_X - DOT_RADIUS + x, SCREEN_CENTER_Y - DOT_RADIUS + y);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Screen2: stats ---
void screen2_label_task(void *param) {
    float y_peak = 0, y_neg = 0, x_total = 0;
    while (1) {
        if (screen2_active) {
            float y = get_gyro_y();
            float x = get_gyro_x();
            if (y > y_peak) y_peak = y;
            if (y < y_neg) y_neg = y;
            x_total += x;

            char buf[32];
            snprintf(buf, sizeof(buf), "Y+: %.2fg", y_peak);
            lv_label_set_text(lbl_peak_y, buf);
            snprintf(buf, sizeof(buf), "Y-: %.2fg", y_neg);
            lv_label_set_text(lbl_neg_y, buf);
            snprintf(buf, sizeof(buf), "X: %.2fg", x_total);
            lv_label_set_text(lbl_total_x, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Screen3: timer ---
void screen3_timer_task(void *param) {
    while (1) {
        if (screen3_active && timer_running) {
            timer_value += 0.05f;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f s", timer_value);
            lv_label_set_text(lbl_timer, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Screen4: stamping trail ---
void screen4_stamp_task(void *param) {
    while (1) {
        if (screen4_active) {
            int16_t gx = (int16_t)get_gyro_x();
            int16_t gy = (int16_t)get_gyro_y();
            lv_obj_t *stamp = lv_img_create(screen4_container);
            lv_img_set_src(stamp, &dot_img);
            lv_obj_set_pos(stamp, SCREEN_CENTER_X + gx, SCREEN_CENTER_Y + gy);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// --- Arduino setup ---
void setup() {
    Serial.begin(115200);
    lv_init();

    // TODO: Initialize display and touch here
    // Example:
    // Display_Init();
    // Touch_Init();

    init_screens();

    // Start tasks
    xTaskCreate(screen1_dot_task, "dot_task", 4096, NULL, 2, NULL);
    xTaskCreate(screen2_label_task, "label_task", 4096, NULL, 2, NULL);
    xTaskCreate(screen3_timer_task, "timer_task", 4096, NULL, 2, NULL);
    xTaskCreate(screen4_stamp_task, "stamp_task", 4096, NULL, 2, NULL);
}

// --- Arduino loop ---
void loop() {
    lv_timer_handler();
    delay(5);
}
