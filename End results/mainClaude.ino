/* Main.ino - 2.5g Visualizer (Optimized for ESP32-S3)
Waveshare ESP32-S3 All-in-One Board
Arduino IDE + LVGL 8.3.11 + SquareLine Studio

Features:
- QMI8658 accelerometer with thread-safe peak tracking
- PCF85063 RTC integration
- SD_MMC buffered logging
- LVGL 8.3 multi-screen interface
- FreeRTOS task management
*/

/* ––––––––––––––
Includes
–––––––––––––– */
#include <Arduino.h>
#include “I2C_Driver.h”
#include “Gyro_QMI8658.h”
#include “RTC_PCF85063.h”
#include “SD_Card.h”
#include “LVGL_Driver.h”
#include “BAT_Driver.h”
#include “Display_ST7701.h”
#include “Touch_CST820.h”
#include “ui.h”

/* ––––––––––––––
External globals from drivers
–––––––––––––– */
extern IMUdata Accel;
extern IMUdata Gyro;
extern datetime_t datetime;

/* ––––––––––––––
Configuration constants
–––––––––––––– */
#define SCREEN_CENTER_X     240
#define SCREEN_CENTER_Y     240
#define DOT_RADIUS          10
#define G_MAX               2.5f
#define STAMP_FADE_MS       700
#define DOT_UPDATE_MS       20
#define STAMP_UPDATE_MS     100
#define LABEL_UPDATE_MS     100
#define MAX_STAMPS          120
#define LOG_BUFFER_SIZE     10
#define BUTTON_DEBOUNCE_MS  200

/* ––––––––––––––
State variables
–––––––––––––– */
// Peak values with mutex protection
static float PeakX = 0.0f, PeakY = 0.0f, PeakZ = 0.0f;
static SemaphoreHandle_t peaksMutex = NULL;

// Timer state
static float timer_value = 0.0f;
static float lap_times[4] = {0, 0, 0, 0};
static bool timer_running = false;
static uint32_t timer_start_ms = 0;

// Button debouncing
static uint32_t last_lap_press = 0;
static uint32_t last_reset_press = 0;

// Screen active flags
static bool screen1_active = false; // GForce
static bool screen2_active = false; // Peaks
static bool screen3_active = false; // Timer
static bool screen4_active = false; // Stamp

// Task handles
static TaskHandle_t dotTaskHandle   = NULL;
static TaskHandle_t labelTaskHandle = NULL;
static TaskHandle_t timerTaskHandle = NULL;
static TaskHandle_t stampTaskHandle = NULL;

// Stamp container & circular buffer
static lv_obj_t *stamp_container = NULL;
static lv_obj_t *stamp_buf[MAX_STAMPS] = {NULL};
static uint16_t stamp_buf_idx = 0;

// SD logging buffer
static char log_buffer[LOG_BUFFER_SIZE][128];
static uint8_t log_buf_count = 0;
static SemaphoreHandle_t logMutex = NULL;

/* ––––––––––––––
Forward declarations
–––––––––––––– */
void screen1_dot_task(void *param);
void screen2_label_task(void *param);
void screen3_timer_task(void *param);
void screen4_stamp_task(void *param);
void manage_tasks(void);
void flush_log_buffer(void);
void SD_Write_String_Buffered(const char *s);
static lv_color_t gforce_to_color(float gx, float gy);

/* ––––––––––––––
Utility Functions
–––––––––––––– */

// Flush buffered log entries to SD card
void flush_log_buffer(void)
{
if (log_buf_count == 0) return;

```
File file = SD_MMC.open("/gforce_log.txt", FILE_APPEND);
if (!file) {
    Serial.println("SD: Failed to open log file");
    log_buf_count = 0; // Discard buffer on error
    return;
}

for (int i = 0; i < log_buf_count; i++) {
    file.print(log_buffer[i]);
}
file.close();
log_buf_count = 0;
```

}

// Buffered SD write with mutex protection
void SD_Write_String_Buffered(const char *data)
{
if (!logMutex) return;

```
if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100))) {
    if (log_buf_count < LOG_BUFFER_SIZE) {
        strncpy(log_buffer[log_buf_count++], data, 127);
        log_buffer[log_buf_count - 1][127] = '\0'; // Ensure null termination
    }
    
    if (log_buf_count >= LOG_BUFFER_SIZE) {
        flush_log_buffer();
    }
    xSemaphoreGive(logMutex);
}
```

}

// Map G-force magnitude to color (green=low, red=high)
static lv_color_t gforce_to_color(float gx, float gy)
{
float mag = sqrtf(gx * gx + gy * gy);
float norm = constrain(mag / G_MAX, 0.0f, 1.0f);
uint8_t r = (uint8_t)(255 * norm);
uint8_t g = (uint8_t)(255 * (1.0f - norm));
return lv_color_make(r, g, 0);
}

/* ––––––––––––––
LVGL screen change hook
–––––––––––––– */
void lv_scr_change_hook(lv_event_t *e)
{
LV_UNUSED(e);
lv_obj_t *act = lv_scr_act();

```
screen1_active = (act == ui_GForceScreen);
screen2_active = (act == ui_PeaksScreen);
screen3_active = (act == ui_TimerScreen);
screen4_active = (act == ui_StampScreen);

// Timer only runs when screen is active
if (!screen3_active) {
    timer_running = false;
}

manage_tasks();
```

}

/* ––––––––––––––
Button callbacks
–––––––––––––– */

// Lap button: Record current time as lap
void lap_button_cb(lv_event_t *e)
{
LV_UNUSED(e);

```
// Debounce
uint32_t now = millis();
if ((now - last_lap_press) < BUTTON_DEBOUNCE_MS) return;
last_lap_press = now;

if (!screen3_active) return;

// Stop timer and record lap
timer_running = false;

// Shift lap times
for (int i = 3; i > 0; --i) {
    lap_times[i] = lap_times[i - 1];
}
lap_times[0] = timer_value;

// Prepare data for LVGL thread
struct LapData {
    char lap1[32];
    char lap2[32];
};

LapData *data = new LapData;
snprintf(data->lap1, 32, "Lap 1: %.2f s", lap_times[0]);
snprintf(data->lap2, 32, "Lap 2: %.2f s", lap_times[1]);

lv_async_call([](void *p) {
    LapData *d = (LapData *)p;
    if (ui_TimeLabel1) lv_label_set_text(ui_TimeLabel1, d->lap1);
    if (ui_TimeLabel2) lv_label_set_text(ui_TimeLabel2, d->lap2);
    delete d;
}, data);
```

}

// Reset button: Clear timer and peaks
void reset_button_cb(lv_event_t *e)
{
LV_UNUSED(e);

```
// Debounce
uint32_t now = millis();
if ((now - last_reset_press) < BUTTON_DEBOUNCE_MS) return;
last_reset_press = now;

// Reset timer
timer_running = false;
timer_value = 0.0f;
timer_start_ms = 0;
for (int i = 0; i < 4; i++) lap_times[i] = 0.0f;

// Reset peaks with mutex
if (peaksMutex && xSemaphoreTake(peaksMutex, pdMS_TO_TICKS(100))) {
    PeakX = 0.0f;
    PeakY = 0.0f;
    PeakZ = 0.0f;
    xSemaphoreGive(peaksMutex);
}

// Update UI
lv_async_call([](void *p) {
    if (ui_timer_label) lv_label_set_text(ui_timer_label, "00:00.00");
    if (ui_TimeLabel1) lv_label_set_text(ui_TimeLabel1, "Lap 1: 0.00 s");
    if (ui_TimeLabel2) lv_label_set_text(ui_TimeLabel2, "Lap 2: 0.00 s");
    if (ui_peakX_label) lv_label_set_text(ui_peakX_label, "Peak X: 0.00");
    if (ui_peakY_label) lv_label_set_text(ui_peakY_label, "Peak Y: 0.00");
    if (ui_peakZ_label) lv_label_set_text(ui_peakZ_label, "Peak Z: 0.00");
}, NULL);

// Flush any pending logs
if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100))) {
    flush_log_buffer();
    xSemaphoreGive(logMutex);
}
```

}

/* ––––––––––––––
Task Management
–––––––––––––– */
void manage_tasks(void)
{
if (dotTaskHandle) {
screen1_active ? vTaskResume(dotTaskHandle) : vTaskSuspend(dotTaskHandle);
}
if (labelTaskHandle) {
screen2_active ? vTaskResume(labelTaskHandle) : vTaskSuspend(labelTaskHandle);
}
if (timerTaskHandle) {
screen3_active ? vTaskResume(timerTaskHandle) : vTaskSuspend(timerTaskHandle);
}
if (stampTaskHandle) {
screen4_active ? vTaskResume(stampTaskHandle) : vTaskSuspend(stampTaskHandle);
}
}

/* ––––––––––––––
Setup
–––––––––––––– */
void setup()
{
Serial.begin(115200);
delay(100);
Serial.println(”\n=== GForce Gauge Booting ===”);

```
// Initialize hardware
Serial.println("Init: I2C...");
I2C_Init();

Serial.println("Init: QMI8658...");
QMI8658_Init();

Serial.println("Init: PCF85063...");
PCF85063_Init();

Serial.println("Init: SD Card...");
SD_Init();

Serial.println("Init: Display...");
Lvgl_Display_Init();

Serial.println("Init: Touch...");
Touch_CST820_Init();

// Initialize LVGL UI
Serial.println("Init: LVGL UI...");
ui_init();

// Create mutexes
peaksMutex = xSemaphoreCreateMutex();
logMutex = xSemaphoreCreateMutex();

if (!peaksMutex || !logMutex) {
    Serial.println("ERROR: Failed to create mutexes!");
}

// Attach screen change events
if (ui_GForceScreen) lv_obj_add_event_cb(ui_GForceScreen, lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
if (ui_PeaksScreen)  lv_obj_add_event_cb(ui_PeaksScreen,  lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
if (ui_TimerScreen)  lv_obj_add_event_cb(ui_TimerScreen,  lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
if (ui_StampScreen)  lv_obj_add_event_cb(ui_StampScreen,  lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);

// Attach button callbacks
if (ui_reset_button) lv_obj_add_event_cb(ui_reset_button, reset_button_cb, LV_EVENT_CLICKED, NULL);
if (ui_LapButton)    lv_obj_add_event_cb(ui_LapButton, lap_button_cb, LV_EVENT_CLICKED, NULL);

// Set stamp container (fallback to screen if no container)
stamp_container = ui_StampContainer ? ui_StampContainer : ui_StampScreen;

if (!stamp_container) {
    Serial.println("WARNING: No stamp container found!");
}

// Load splash screen, then transition to GForce screen
if (ui_SplashScreen) {
    lv_scr_load(ui_SplashScreen);
    lv_timer_create([](lv_timer_t *t) {
        if (ui_GForceScreen) {
            lv_scr_load_anim(ui_GForceScreen, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
        }
        lv_timer_del(t);
    }, 2000, NULL);
} else if (ui_GForceScreen) {
    lv_scr_load(ui_GForceScreen);
}

// Create FreeRTOS tasks (increased stack for stability)
Serial.println("Creating tasks...");
xTaskCreatePinnedToCore(screen1_dot_task,   "dot_task",   8192, NULL, 2, &dotTaskHandle,   1);
xTaskCreatePinnedToCore(screen2_label_task, "label_task", 8192, NULL, 2, &labelTaskHandle, 1);
xTaskCreatePinnedToCore(screen3_timer_task, "timer_task", 8192, NULL, 2, &timerTaskHandle, 1);
xTaskCreatePinnedToCore(screen4_stamp_task, "stamp_task", 8192, NULL, 2, &stampTaskHandle, 1);

// Start all tasks suspended (will resume when screen becomes active)
vTaskSuspend(dotTaskHandle);
vTaskSuspend(labelTaskHandle);
vTaskSuspend(timerTaskHandle);
vTaskSuspend(stampTaskHandle);

Serial.println("=== Initialization Complete ===\n");
```

}

/* ––––––––––––––
Main Loop
–––––––––––––– */
void loop()
{
Lvgl_Loop();
vTaskDelay(pdMS_TO_TICKS(5));
}

/* ––––––––––––––
Screen Tasks
–––––––––––––– */

// 1️⃣ G-Force Dot Visualization Task
void screen1_dot_task(void *param)
{
(void)param;

```
const float alpha = 0.15f; // Low-pass filter coefficient
float fx = 0.0f, fy = 0.0f;

while (1) {
    // Read accelerometer
    QMI8658_Loop();
    float gx = Accel.x;
    float gy = Accel.y;
    float gz = Accel.z;

    // Update peak values (thread-safe)
    if (peaksMutex && xSemaphoreTake(peaksMutex, pdMS_TO_TICKS(10))) {
        if (fabsf(gx) > PeakX) PeakX = fabsf(gx);
        if (fabsf(gy) > PeakY) PeakY = fabsf(gy);
        if (fabsf(gz) > PeakZ) PeakZ = fabsf(gz);
        xSemaphoreGive(peaksMutex);
    }

    // Apply low-pass filter for smooth motion
    fx = fx * (1.0f - alpha) + gx * alpha;
    fy = fy * (1.0f - alpha) + gy * alpha;

    // Map to screen coordinates
    int16_t px = (int16_t)constrain((fx / G_MAX) * 200.0f, -200.0f, 200.0f);
    int16_t py = (int16_t)constrain((fy / G_MAX) * 200.0f, -200.0f, 200.0f);

    // Prepare data for LVGL thread
    struct DotPos {
        int16_t x, y;
        lv_color_t color;
    };
    
    DotPos *pos = new DotPos;
    pos->x = px;
    pos->y = py;
    pos->color = gforce_to_color(fx, fy);

    lv_async_call([](void *p) {
        DotPos *dp = (DotPos *)p;
        if (ui_gforce_dot) {
            lv_obj_set_pos(ui_gforce_dot, 
                           SCREEN_CENTER_X - DOT_RADIUS + dp->x,
                           SCREEN_CENTER_Y - DOT_RADIUS + dp->y);
            lv_obj_set_style_bg_color(ui_gforce_dot, dp->color, LV_PART_MAIN);
        }
        delete dp;
    }, pos);

    vTaskDelay(pdMS_TO_TICKS(DOT_UPDATE_MS));
}
```

}

// 2️⃣ Peak Values Display Task
void screen2_label_task(void *param)
{
(void)param;

```
char buf[96]; // Single buffer for efficiency

while (1) {
    // Read peaks (thread-safe)
    float px, py, pz;
    if (peaksMutex && xSemaphoreTake(peaksMutex, pdMS_TO_TICKS(100))) {
        px = PeakX;
        py = PeakY;
        pz = PeakZ;
        xSemaphoreGive(peaksMutex);
    } else {
        vTaskDelay(pdMS_TO_TICKS(LABEL_UPDATE_MS));
        continue;
    }

    // Format strings
    struct PeakData {
        char x[32], y[32], z[32];
    };
    
    PeakData *data = new PeakData;
    snprintf(data->x, 32, "Peak X: %.2f", px);
    snprintf(data->y, 32, "Peak Y: %.2f", py);
    snprintf(data->z, 32, "Peak Z: %.2f", pz);

    lv_async_call([](void *p) {
        PeakData *pd = (PeakData *)p;
        if (ui_peakX_label) lv_label_set_text(ui_peakX_label, pd->x);
        if (ui_peakY_label) lv_label_set_text(ui_peakY_label, pd->y);
        if (ui_peakZ_label) lv_label_set_text(ui_peakZ_label, pd->z);
        delete pd;
    }, data);

    vTaskDelay(pdMS_TO_TICKS(LABEL_UPDATE_MS));
}
```

}

// 3️⃣ Timer Display Task
void screen3_timer_task(void *param)
{
(void)param;

```
while (1) {
    uint32_t now = millis();
    
    if (timer_running) {
        if (timer_start_ms == 0) {
            timer_start_ms = now;
        }
        // Calculate from start time to avoid accumulation error
        timer_value = (now - timer_start_ms) / 1000.0f;
        
        // Format time as MM:SS.CS
        unsigned long total_cs = (unsigned long)(timer_value * 100.0f);
        unsigned int cs = total_cs % 100;
        unsigned long tot_s = total_cs / 100;
        unsigned int s = tot_s % 60;
        unsigned int m = (tot_s / 60) % 1000;
        
        char *txt = new char[32];
        snprintf(txt, 32, "%02u:%02u.%02u", m, s, cs);
        
        lv_async_call([](void *p) {
            char *s = (char *)p;
            if (ui_timer_label) lv_label_set_text(ui_timer_label, s);
            delete[] s;
        }, txt);
    } else {
        timer_start_ms = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}
```

}

// 4️⃣ G-Force Stamp Visualization Task
void screen4_stamp_task(void *param)
{
(void)param;

```
if (!stamp_container) {
    Serial.println("ERROR: No stamp container, deleting task");
    vTaskDelete(NULL);
    return;
}

while (1) {
    // Read accelerometer and RTC
    QMI8658_Loop();
    PCF85063_Read_Time(&datetime);

    float gx = Accel.x;
    float gy = Accel.y;
    
    // Map to screen coordinates
    int16_t sx = (int16_t)constrain((gx / G_MAX) * 200.0f, -200.0f, 200.0f);
    int16_t sy = (int16_t)constrain((gy / G_MAX) * 200.0f, -200.0f, 200.0f);
    lv_color_t color = gforce_to_color(gx, gy);

    // Format timestamp
    char ts_buf[64];
    datetime_to_str(ts_buf, datetime);

    // Prepare stamp data
    struct StampInfo {
        int16_t sx, sy;
        lv_color_t col;
        char ts[64];
        float gx, gy;
    };
    
    StampInfo *si = (StampInfo *)malloc(sizeof(StampInfo));
    si->sx = sx;
    si->sy = sy;
    si->col = color;
    strncpy(si->ts, ts_buf, 63);
    si->ts[63] = '\0';
    si->gx = gx;
    si->gy = gy;

    lv_async_call([](void *p) {
        StampInfo *s = (StampInfo *)p;
        
        // Delete old stamp if buffer is full
        if (stamp_buf[stamp_buf_idx] != NULL) {
            lv_obj_del(stamp_buf[stamp_buf_idx]);
            stamp_buf[stamp_buf_idx] = NULL;
        }
        
        // Create new stamp
        lv_obj_t *st = lv_obj_create(stamp_container);
        lv_obj_set_size(st, 6, 6);
        lv_obj_set_style_radius(st, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(st, s->col, 0);
        lv_obj_set_style_border_width(st, 0, 0);
        lv_obj_set_style_opa(st, LV_OPA_COVER, 0);
        lv_obj_set_pos(st, SCREEN_CENTER_X - 3 + s->sx, SCREEN_CENTER_Y - 3 + s->sy);

        // Add to circular buffer
        stamp_buf[stamp_buf_idx] = st;
        stamp_buf_idx = (stamp_buf_idx + 1) % MAX_STAMPS;

        // Fade out animation
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, st);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&a, STAMP_FADE_MS);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_ready_cb(&a, [](lv_anim_t *a) {
            // Note: Object deleted by circular buffer, not here
            // to prevent double-free
        });
        lv_anim_start(&a);

        // Log to SD card (buffered)
        char logline[128];
        snprintf(logline, sizeof(logline), "%s,%.3f,%.3f\n", s->ts, s->gx, s->gy);
        SD_Write_String_Buffered(logline);

        free(s);
    }, si);

    vTaskDelay(pdMS_TO_TICKS(STAMP_UPDATE_MS));
}
```

}
