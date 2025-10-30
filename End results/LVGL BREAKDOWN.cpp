/*
  GForce Gauge ESP32-S3 (Waveshare 2.1" Round) - Thread-Safe + ±2.5g
  --------------------------------------------------------------------
  - LVGL-safe via lv_async_call() for all UI updates
  - Tasks suspended/resumed based on active screen
  - Dot motion scaled to ±2.5 g
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
#include "ui.h"

// --- Constants ---
#define SCREEN_CENTER_X 240
#define SCREEN_CENTER_Y 240
#define DOT_RADIUS      10
#define G_MAX           2.5f   // ±2.5g range

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

// --- Task handles ---
TaskHandle_t dotTaskHandle   = NULL;
TaskHandle_t labelTaskHandle = NULL;
TaskHandle_t timerTaskHandle = NULL;
TaskHandle_t stampTaskHandle = NULL;

// --- Forward declarations ---
void screen1_dot_task(void *param);
void screen2_label_task(void *param);
void screen3_timer_task(void *param);
void screen4_stamp_task(void *param);
void manage_tasks(void);

// --- Gyro helpers ---
float get_gyro_x(void);
float get_gyro_y(void);

// --- Screen change hook ---
void lv_scr_change_hook(lv_event_t *e) {
    lv_obj_t *act = lv_scr_act();

    screen1_active = (act == ui_Screen1);
    screen2_active = (act == ui_Screen2);
    screen3_active = (act == ui_Screen3);
    screen4_active = (act == ui_Screen4);

    timer_running = screen3_active;
    manage_tasks();  // suspend/resume as needed
}

// --- Lap button ---
void lap_button_cb(lv_event_t *e) {
    if (!screen3_active) return;

    lap_times[lap_idx] = timer_value;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f s", lap_times[lap_idx]);

    // Safe LVGL update
    lv_async_call([](void *p) {
        lv_label_set_text(ui_LapLabels[((int)p)], ((char*)lv_event_get_user_data(lv_event_get_current())));
    }, NULL);

    lv_label_set_text(ui_LapLabels[lap_idx], buf);
    lap_idx = (lap_idx + 1) % 4;
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
}

// --- Task manager ---
void manage_tasks() {
    if (dotTaskHandle)   (screen1_active ? vTaskResume(dotTaskHandle)   : vTaskSuspend(dotTaskHandle));
    if (labelTaskHandle) (screen2_active ? vTaskResume(labelTaskHandle) : vTaskSuspend(labelTaskHandle));
    if (timerTaskHandle) (screen3_active ? vTaskResume(timerTaskHandle) : vTaskSuspend(timerTaskHandle));
    if (stampTaskHandle) (screen4_active ? vTaskResume(stampTaskHandle) : vTaskSuspend(stampTaskHandle));
}

// --- Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("GForce Gauge Booting...");

    I2C_Init();
    Gyro_Init();

    lv_init();
    Lvgl_Display_Init();
    Touch_CST820_Init();

    ui_init();

    // Hook events
    lv_obj_add_event_cb(ui_Screen1, lv_scr_change_hook, LV_EVENT_SCREEN_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Screen2, lv_scr_change_hook, LV_EVENT_SCREEN_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Screen3, lv_scr_change_hook, LV_EVENT_SCREEN_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Screen4, lv_scr_change_hook, LV_EVENT_SCREEN_CHANGED, NULL);

    lv_obj_add_event_cb(ui_LapButton, lap_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ResetButton, reset_button_cb, LV_EVENT_CLICKED, NULL);

    // Tasks (start suspended)
    xTaskCreatePinnedToCore(screen1_dot_task,   "dot_task",   4096, NULL, 2, &dotTaskHandle,   1);
    xTaskCreatePinnedToCore(screen2_label_task, "label_task", 4096, NULL, 2, &labelTaskHandle, 1);
    xTaskCreatePinnedToCore(screen3_timer_task, "timer_task", 4096, NULL, 2, &timerTaskHandle, 1);
    xTaskCreatePinnedToCore(screen4_stamp_task, "stamp_task", 4096, NULL, 2, &stampTaskHandle, 1);

    manage_tasks();
    Serial.println("UI initialized — running.");
}

void loop() {
    lv_timer_handler();
    delay(5);
}

// --- Screen 1: Moving dot (±2.5g mapped) ---
void screen1_dot_task(void *param) {
    static float fx = 0, fy = 0;
    const float alpha = 0.15f;

    while (1) {
        float gx = get_gyro_x();
        float gy = get_gyro_y();

        fx = fx * (1 - alpha) + gx * alpha;
        fy = fy * (1 - alpha) + gy * alpha;

        // Map ±2.5g to ±200 px movement
        int16_t x = constrain((int16_t)((fx / G_MAX) * 200), -200, 200);
        int16_t y = constrain((int16_t)((fy / G_MAX) * 200), -200, 200);

        lv_async_call([](void *p) {
            int16_t *pos = (int16_t *)p;
            lv_obj_set_pos(ui_DotImage, SCREEN_CENTER_X - DOT_RADIUS + pos[0], SCREEN_CENTER_Y - DOT_RADIUS + pos[1]);
            delete[] pos;
        }, new int16_t[2]{x, y});

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Screen 2: Peak/Neg/Total Stats ---
void screen2_label_task(void *param) {
    float y_peak = 0, y_neg = 0, x_total = 0;

    while (1) {
        float y = get_gyro_y();
        float x = get_gyro_x();
        if (y > y_peak) y_peak = y;
        if (y < y_neg) y_neg = y;
        x_total += x * 0.05f; // integrate roughly over time

        char buf_peak[32], buf_neg[32], buf_total[32];
        snprintf(buf_peak, sizeof(buf_peak), "Y+: %.2fg", y_peak);
        snprintf(buf_neg,  sizeof(buf_neg),  "Y-: %.2fg", y_neg);
        snprintf(buf_total,sizeof(buf_total),"XΣ: %.2fg", x_total);

        lv_async_call([](void *p) {
            char **data = (char **)p;
            lv_label_set_text(ui_LabelPeakY, data[0]);
            lv_label_set_text(ui_LabelNegY,  data[1]);
            lv_label_set_text(ui_LabelTotalX,data[2]);
            delete[] data[0]; delete[] data[1]; delete[] data[2]; delete[] data;
        }, ({
            char **arr = new char*[3];
            arr[0] = strdup(buf_peak);
            arr[1] = strdup(buf_neg);
            arr[2] = strdup(buf_total);
            arr;
        }));

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Screen 3: Timer ---
void screen3_timer_task(void *param) {
    uint32_t last_ms = millis();

    while (1) {
        if (timer_running) {
            uint32_t now = millis();
            timer_value += (now - last_ms) / 1000.0f;
            last_ms = now;

            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f s", timer_value);

            lv_async_call([](void *p) {
                lv_label_set_text(ui_TimerLabel, (char*)p);
                delete[] (char*)p;
            }, strdup(buf));
        } else {
            last_ms = millis(); // reset drift
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Screen 4: Dot trail (limited to 100 stamps) ---
void screen4_stamp_task(void *param) {
    while (1) {
        int16_t gx = (int16_t)(get_gyro_x() / G_MAX * 200);
        int16_t gy = (int16_t)(get_gyro_y() / G_MAX * 200);

        lv_async_call([](void *p) {
            int16_t *pos = (int16_t*)p;
            lv_obj_t *stamp = lv_img_create(ui_Screen4Container);
            lv_img_set_src(stamp, &ui_DotImg);
            lv_obj_set_pos(stamp, SCREEN_CENTER_X + pos[0], SCREEN_CENTER_Y + pos[1]);

            if (lv_obj_get_child_cnt(ui_Screen4Container) > 100)
                lv_obj_del(lv_obj_get_child(ui_Screen4Container, 0));

            delete[] pos;
        }, new int16_t[2]{gx, gy});

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
