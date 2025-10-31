/* Main.ino - 2.5g Visualizer integrated with your drivers
   Uses:
    - QMI8658 accelerometer (getAccelerometer called from QMI8658_Loop())
    - PCF85063 RTC (datetime_t datetime global provided by RTC driver)
    - SD_MMC (SD_Card.h)
    - LVGL + SquareLine ui.h (ui_init() and ui_* objects)
*/

/* ----------------------------
   Includes (your offline drivers)
   ---------------------------- */
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

/* ----------------------------
   External globals from drivers
   ---------------------------- */
extern IMUdata Accel;
extern IMUdata Gyro;
extern datetime_t datetime;

/* ----------------------------
   Configuration constants
   ---------------------------- */
#define SCREEN_CENTER_X 240
#define SCREEN_CENTER_Y 240
#define DOT_RADIUS      10
#define G_MAX           2.5f
#define STAMP_FADE_MS   700
#define DOT_UPDATE_MS   20
#define STAMP_UPDATE_MS 100
#define LABEL_UPDATE_MS 100
#define MAX_STAMPS      120

/* ----------------------------
   State variables
   ---------------------------- */
volatile float PeakX = 0.0f, PeakY = 0.0f, PeakZ = 0.0f;
float timer_value = 0.0f;
float lap_times[4] = {0,0,0,0};
bool timer_running = false;

/* Screen active flags */
bool screen1_active = false; // GForce
bool screen2_active = false; // Peaks
bool screen3_active = false; // Timer
bool screen4_active = false; // Stamp

/* Task handles */
TaskHandle_t dotTaskHandle   = NULL;
TaskHandle_t labelTaskHandle = NULL;
TaskHandle_t timerTaskHandle = NULL;
TaskHandle_t stampTaskHandle = NULL;

/* Stamp container & buffer */
lv_obj_t *stamp_container = NULL;
static lv_obj_t *stamp_buf[MAX_STAMPS];
static uint16_t stamp_buf_idx = 0;

/* ----------------------------
   Forward declarations
   ---------------------------- */
void screen1_dot_task(void *param);
void screen2_label_task(void *param);
void screen3_timer_task(void *param);
void screen4_stamp_task(void *param);
void manage_tasks(void);
void SD_Write_String(const char *s);
static lv_color_t gforce_to_color(float gx, float gy);
static void write_stamp_log_to_sd(const char *ts, float gx, float gy);

/* ----------------------------
   Utility Implementations
   ---------------------------- */

void SD_Write_String(const char *data)
{
    File file = SD_MMC.open("/gforce_log.txt", FILE_APPEND);
    if (!file) {
        printf("SD: Failed to open /gforce_log.txt for append\r\n");
        return;
    }
    file.print(data);
    file.close();
}

static lv_color_t gforce_to_color(float gx, float gy)
{
    float mag = sqrtf(gx*gx + gy*gy);
    float norm = constrain(mag / G_MAX, 0.0f, 1.0f);
    uint8_t r = (uint8_t)(255 * norm);
    uint8_t g = (uint8_t)(255 * (1.0f - norm));
    return lv_color_make(r,g,0);
}

/* ----------------------------
   LVGL screen change hook
   ---------------------------- */
void lv_scr_change_hook(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_obj_t *act = lv_scr_act();

    screen1_active = (act == ui_GForceScreen);
    screen2_active = (act == ui_PeaksScreen);
    screen3_active = (act == ui_TimerScreen);
    screen4_active = (act == ui_StampScreen);

    timer_running = screen3_active;
    manage_tasks();
}

/* ----------------------------
   Button callbacks
   ---------------------------- */
void lap_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!screen3_active) return;

    timer_running = false;
    for (int i = 3; i > 0; --i) lap_times[i] = lap_times[i-1];
    lap_times[0] = timer_value;

    char buf1[32], buf2[32];
    snprintf(buf1, sizeof(buf1), "Lap 1: %.2f s", lap_times[0]);
    snprintf(buf2, sizeof(buf2), "Lap 2: %.2f s", lap_times[1]);

    char *c1 = strdup(buf1);
    char *c2 = strdup(buf2);
    lv_async_call([](void *p){
        char **arr = (char **)p;
        if (ui_TimeLabel1) lv_label_set_text(ui_TimeLabel1, arr[0]);
        if (ui_TimeLabel2) lv_label_set_text(ui_TimeLabel2, arr[1]);
        free(arr[0]); free(arr[1]); free(arr);
    }, ({
        char **arr = (char**)malloc(sizeof(char*)*2);
        arr[0]=c1; arr[1]=c2; arr;
    }));
}

void reset_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    timer_running = false;
    timer_value = 0.0f;
    for (int i=0;i<4;i++) lap_times[i]=0.0f;

    lv_async_call([](void *p){
        if (ui_timer_label) lv_label_set_text(ui_timer_label, "00:00.00");
        if (ui_peakX_label) lv_label_set_text(ui_peakX_label, "0.00");
        if (ui_peakY_label) lv_label_set_text(ui_peakY_label, "0.00");
        if (ui_peakZ_label) lv_label_set_text(ui_peakZ_label, "0.00");
    }, NULL);
}

/* ----------------------------
   Manage screen tasks
   ---------------------------- */
void manage_tasks(void)
{
    if (dotTaskHandle)   (screen1_active) ? vTaskResume(dotTaskHandle)   : vTaskSuspend(dotTaskHandle);
    if (labelTaskHandle) (screen2_active) ? vTaskResume(labelTaskHandle) : vTaskSuspend(labelTaskHandle);
    if (timerTaskHandle) (screen3_active) ? vTaskResume(timerTaskHandle) : vTaskSuspend(timerTaskHandle);
    if (stampTaskHandle) (screen4_active) ? vTaskResume(stampTaskHandle) : vTaskSuspend(stampTaskHandle);
}

/* ----------------------------
   Setup
   ---------------------------- */
void setup()
{
    Serial.begin(115200);
    Serial.println("GForce Gauge Booting...");

    I2C_Init();
    QMI8658_Init();
    PCF85063_Init();
    SD_Init();
    Lvgl_Display_Init();
    Touch_CST820_Init();

    ui_init();

    // Attach screen change events
    lv_obj_add_event_cb(ui_GForceScreen, lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_PeaksScreen,  lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_TimerScreen,  lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_StampScreen,  lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);

    // Button callbacks
    if (ui_reset_button) lv_obj_add_event_cb(ui_reset_button, reset_button_cb, LV_EVENT_CLICKED, NULL);
    if (ui_LapButton)    lv_obj_add_event_cb(ui_LapButton, lap_button_cb, LV_EVENT_CLICKED, NULL);

    // Container for stamps
    stamp_container = ui_StampContainer ? ui_StampContainer : ui_StampScreen;

    // Load splash screen then fade to GForce
    lv_scr_load(ui_SplashScreen);
    lv_timer_create([](lv_timer_t *t){
        lv_scr_load_anim(ui_GForceScreen, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
        lv_timer_del(t);
    }, 2000, NULL);

    // Create and suspend tasks
    xTaskCreatePinnedToCore(screen1_dot_task,   "dot_task",   4096, NULL, 2, &dotTaskHandle,   1);
    xTaskCreatePinnedToCore(screen2_label_task, "label_task", 4096, NULL, 2, &labelTaskHandle, 1);
    xTaskCreatePinnedToCore(screen3_timer_task, "timer_task", 4096, NULL, 2, &timerTaskHandle, 1);
    xTaskCreatePinnedToCore(screen4_stamp_task, "stamp_task", 4096, NULL, 2, &stampTaskHandle, 1);

    vTaskSuspend(dotTaskHandle);
    vTaskSuspend(labelTaskHandle);
    vTaskSuspend(timerTaskHandle);
    vTaskSuspend(stampTaskHandle);

    Serial.println("Initialization complete.");
}

/* ----------------------------
   Loop
   ---------------------------- */
void loop()
{
    Lvgl_Loop();
    vTaskDelay(pdMS_TO_TICKS(5));
}

/* ----------------------------
   Screen Tasks
   ---------------------------- */

// 1️⃣ G-Force Dot Task
void screen1_dot_task(void *param)
{
    (void)param;
    const float alpha = 0.15f;
    float fx = 0.0f, fy = 0.0f;

    while (1) {
        QMI8658_Loop();
        float gx = Accel.x, gy = Accel.y, gz = Accel.z;

        if (fabsf(gx) > PeakX) PeakX = fabsf(gx);
        if (fabsf(gy) > PeakY) PeakY = fabsf(gy);
        if (fabsf(gz) > PeakZ) PeakZ = fabsf(gz);

        fx = fx * (1.0f - alpha) + gx * alpha;
        fy = fy * (1.0f - alpha) + gy * alpha;

        int16_t px = (int16_t)constrain((fx / G_MAX) * 200.0f, -200.0f, 200.0f);
        int16_t py = (int16_t)constrain((fy / G_MAX) * 200.0f, -200.0f, 200.0f);

        int16_t *pos = new int16_t[2];
        pos[0] = px; pos[1] = py;

        lv_async_call([](void *p){
            int16_t *pp = (int16_t*)p;
            if (ui_gforce_dot) {
                lv_obj_set_pos(ui_gforce_dot, SCREEN_CENTER_X - DOT_RADIUS + pp[0],
                                                 SCREEN_CENTER_Y - DOT_RADIUS + pp[1]);
                lv_obj_set_style_bg_color(ui_gforce_dot,
                    gforce_to_color((float)pp[0]/200.0f*G_MAX, (float)pp[1]/200.0f*G_MAX),
                    LV_PART_MAIN);
            }
            delete[] pp;
        }, pos);

        vTaskDelay(pdMS_TO_TICKS(DOT_UPDATE_MS));
    }
}

// 2️⃣ Peaks Screen Task
void screen2_label_task(void *param)
{
    (void)param;
    while (1) {
        char bufx[32], bufy[32], bufz[32];
        snprintf(bufx, sizeof(bufx), "Peak X: %.2f", PeakX);
        snprintf(bufy, sizeof(bufy), "Peak Y: %.2f", PeakY);
        snprintf(bufz, sizeof(bufz), "Peak Z: %.2f", PeakZ);

        char *p1 = strdup(bufx), *p2 = strdup(bufy), *p3 = strdup(bufz);
        lv_async_call([](void *p){
            char **arr = (char**)p;
            if (ui_peakX_label) lv_label_set_text(ui_peakX_label, arr[0]);
            if (ui_peakY_label) lv_label_set_text(ui_peakY_label, arr[1]);
            if (ui_peakZ_label) lv_label_set_text(ui_peakZ_label, arr[2]);
            free(arr[0]); free(arr[1]); free(arr[2]); free(arr);
        }, ({
            char **arr = (char**)malloc(sizeof(char*)*3);
            arr[0]=p1; arr[1]=p2; arr[2]=p3; arr;
        }));
        vTaskDelay(pdMS_TO_TICKS(LABEL_UPDATE_MS));
    }
}

// 3️⃣ Timer Screen Task
void screen3_timer_task(void *param)
{
    (void)param;
    uint32_t last_ms = millis();

    while (1) {
        uint32_t now = millis();
        if (timer_running) {
            timer_value += (now - last_ms) / 1000.0f;
            unsigned long total_cs = (unsigned long)(timer_value * 100.0f);
            unsigned int cs = total_cs % 100;
            unsigned long tot_s = total_cs / 100;
            unsigned int s = tot_s % 60;
            unsigned int m = (tot_s / 60) % 1000;
            char buf[32];
            snprintf(buf, sizeof(buf), "%02u:%02u.%02u", m, s, cs);
            char *txt = strdup(buf);
            lv_async_call([](void *p){
                char *s = (char*)p;
                if (ui_timer_label) lv_label_set_text(ui_timer_label, s);
                free(s);
            }, txt);
        }
        last_ms = now;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 4️⃣ Stamp Screen Task
void screen4_stamp_task(void *param)
{
    (void)param;
    while (1) {
        QMI8658_Loop();
        PCF85063_Read_Time(&datetime);

        float gx = Accel.x;
        float gy = Accel.y;
        int16_t sx = (int16_t)constrain((gx / G_MAX) * 200.0f, -200.0f, 200.0f);
        int16_t sy = (int16_t)constrain((gy / G_MAX) * 200.0f, -200.0f, 200.0f);
        lv_color_t color = gforce_to_color(gx, gy);

        char ts_buf[64];
        datetime_to_str(ts_buf, datetime);

        struct StampInfo {
            int16_t sx, sy;
            lv_color_t col;
            char ts[64];
            float gx, gy;
        };
        StampInfo *si = (StampInfo*)malloc(sizeof(StampInfo));
        si->sx = sx; si->sy = sy; si->col = color;
        strcpy(si->ts, ts_buf);
        si->gx = gx; si->gy = gy;

        lv_async_call([](void *p){
            StampInfo *s = (StampInfo*)p;
            lv_obj_t *st = lv_obj_create(stamp_container);
            lv_obj_set_size(st, 6, 6);
            lv_obj_set_style_radius(st, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(st, s->col, 0);
            lv_obj_set_style_border_width(st, 0, 0);
            lv_obj_set_style_opa(st, LV_OPA_COVER, 0);
            lv_obj_set_pos(st, SCREEN_CENTER_X - 3 + s->sx, SCREEN_CENTER_Y - 3 + s->sy);

            stamp_buf[stamp_buf_idx] = st;
            stamp_buf_idx = (stamp_buf_idx + 1) % MAX_STAMPS;

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, st);
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_time(&a, STAMP_FADE_MS);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_ready_cb(&a, [](lv_anim_t * a){ lv_obj_del((lv_obj_t*)a->var); });
            lv_anim_start(&a);

            char logline[128];
            snprintf(logline, sizeof(logline), "%s,%.3f,%.3f\n", s->ts, s->gx, s->gy);
            SD_Write_String(logline);

            free(s);
        }, si);

        vTaskDelay(pdMS_TO_TICKS(STAMP_UPDATE_MS));
    }
}
