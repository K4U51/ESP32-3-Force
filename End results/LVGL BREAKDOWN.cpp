/*
  GForce Gauge ESP32-S3 (Waveshare 2.1" Round) - SquareLine UI Version
  --------------------------------------------------------------------
  - Uses your existing LVGL, Display, and Touch drivers.
  - Loads UI designed in SquareLine (ui.c / ui.h).
  - 4 screens:
      1. Moving dot (gyro)
      2. Peak/negative/total tracker
      3. Timer + Lap/Reset
      4. Trail of dot stamps
*/

#include <Arduino.h>
#include "I2C_Driver.h"
#include "Gyro_QMI8658.h"
#include "RTC_PCF85063.h"
#include "SD_Card.h"
#include "LVGL_Driver.h"
#include "BAT_Driver.h"
#include "Display_ST7701.h"
#include "Touch_CST820.h"
#include "ui.h"  // <-- Only new header added

// --- Gyro helpers ---
float get_gyro_x(void);
float get_gyro_y(void);

// --- Runtime variables ---
float timer_value = 0;
float lap_times[4] = {0};
int lap_idx = 0;
bool timer_running = false;

// --- Screen flags ---
bool screen1_active = false;
bool screen2_active = false;
bool screen3_active = false;
bool screen4_active = false;

// --- Constants ---
#define SCREEN_CENTER_X 240
#define SCREEN_CENTER_Y 240
#define DOT_RADIUS      10

// --- Forward declarations ---
void screen1_dot_task(void *param);
void screen2_label_task(void *param);
void screen3_timer_task(void *param);
void screen4_stamp_task(void *param);

// --- Screen change hook ---
void lv_scr_change_hook(lv_event_t *e) {
    lv_obj_t *act = lv_scr_act();

    screen1_active = (act == ui_Screen1);
    screen2_active = (act == ui_Screen2);
    screen3_active = (act == ui_Screen3);
    screen4_active = (act == ui_Screen4);

    timer_running = screen3_active;
}

// --- Lap button ---
void lap_button_cb(lv_event_t *e) {
    if (!screen3_active) return;

    lap_times[lap_idx] = timer_value;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f s", lap_times[lap_idx]);
    lv_label_set_text(ui_LapLabels[lap_idx], buf);
    lap_idx = (lap_idx + 1) % 4;
    lv_event_set_ready(e);
}

// --- Reset button ---
void reset_button_cb(lv_event_t *e) {
    timer_value = 0;
    lap_idx = 0;
    for (int i = 0; i < 4; i++) {
        lap_times[i] = 0;
        lv_label_set_text(ui_LapLabels[i], "--:--.--");
    }
    lv_label_set_text(ui_TimerLabel, "0.00 s");
    lv_event_set_ready(e);
}

// --- Arduino setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("GForce Gauge Booting...");

    I2C_Init();
    Gyro_Init();

    lv_init();
    Lvgl_Display_Init();   // sets up draw buffers and ST7701 driver
    Touch_CST820_Init();   // registers touch input driver

    // --- Load SquareLine UI ---
    ui_init();

    // --- Optional: Add callbacks (if not already set in SquareLine) ---
    lv_obj_add_event_cb(lv_scr_act(), lv_scr_change_hook, LV_EVENT_SCREEN_CHANGED, NULL);
    lv_obj_add_event_cb(ui_LapButton, lap_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ResetButton, reset_button_cb, LV_EVENT_CLICKED, NULL);

    // --- Start background tasks ---
    xTaskCreatePinnedToCore(screen1_dot_task, "dot_task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(screen2_label_task, "label_task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(screen3_timer_task, "timer_task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(screen4_stamp_task, "stamp_task", 4096, NULL, 2, NULL, 1);

    Serial.println("UI initialized — running.");
}

// --- Main loop ---
void loop() {
    lv_timer_handler();
    delay(5);
}

// --- Screen1: Moving dot (gyro) ---
void screen1_dot_task(void *param) {
    static float filtered_x = 0, filtered_y = 0;
    const float alpha = 0.15f;

    while (1) {
        if (screen1_active) {
            float gx = get_gyro_x();
            float gy = get_gyro_y();

            filtered_x = filtered_x * (1 - alpha) + gx * alpha;
            filtered_y = filtered_y * (1 - alpha) + gy * alpha;

            int16_t x = (int16_t)(filtered_x * 100);
            int16_t y = (int16_t)(filtered_y * 100);

            lv_obj_set_pos(ui_DotImage, SCREEN_CENTER_X - DOT_RADIUS + x, SCREEN_CENTER_Y - DOT_RADIUS + y);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Screen2: Stats display ---
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
            lv_label_set_text(ui_LabelPeakY, buf);
            snprintf(buf, sizeof(buf), "Y-: %.2fg", y_neg);
            lv_label_set_text(ui_LabelNegY, buf);
            snprintf(buf, sizeof(buf), "X: %.2fg", x_total);
            lv_label_set_text(ui_LabelTotalX, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Screen3: Timer ---
void screen3_timer_task(void *param) {
    while (1) {
        if (screen3_active && timer_running) {
            timer_value += 0.05f;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f s", timer_value);
            lv_label_set_text(ui_TimerLabel, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Screen4: Dot trail ---
void screen4_stamp_task(void *param) {
    while (1) {
        if (screen4_active) {
            int16_t gx = (int16_t)get_gyro_x();
            int16_t gy = (int16_t)get_gyro_y();

            lv_obj_t *stamp = lv_img_create(ui_Screen4Container);
            lv_img_set_src(stamp, &ui_DotImg);
            lv_obj_set_pos(stamp, SCREEN_CENTER_X + gx, SCREEN_CENTER_Y + gy);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
