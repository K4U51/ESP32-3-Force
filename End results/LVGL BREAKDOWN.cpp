/* Main.ino
   2.5g Visualizer with LVGL + SquareLine UI integration
   - Keep your drivers and offline libs as-is
   - Map ±2.5g to UI, stamp trail with fade, log X,Y,timestamp to SD
   - Only run tasks when screen active
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
#define MAX_STAMPS      100
#define STAMP_FADE_MS   700    // fade-out time for each stamp
#define DOT_UPDATE_MS   20
#define LABEL_UPDATE_MS 50
#define TIMER_UPDATE_MS 50
#define STAMP_UPDATE_MS 200

// --- Runtime variables ---
float timer_value = 0.0f;
float lap_times[4] = {0.0f,0.0f,0.0f,0.0f};
int lap_idx = 0;
bool timer_running = false;

// Sensor / peaks
volatile float PeakX = 0.0f, PeakY = 0.0f, PeakZ = 0.0f;

// Screen actives
bool screen1_active = false;
bool screen2_active = false;
bool screen3_active = false;
bool screen4_active = false;

// Task handles
TaskHandle_t dotTaskHandle   = NULL;
TaskHandle_t labelTaskHandle = NULL;
TaskHandle_t timerTaskHandle = NULL;
TaskHandle_t stampTaskHandle = NULL;

// Stamp layer container (SquareLine screen container or object)
lv_obj_t *stamp_container = NULL;

// Circular buffer for stamps (optional tracking)
static lv_obj_t *stamp_buf[MAX_STAMPS];
static uint16_t stamp_buf_idx = 0;

// UI initial states (ensure these names match your SquareLine export)
const char INITIAL_TIMER_TEXT[] = "00:00.00";
const char INITIAL_PEAK_TEXT[]  = "0.00";

// --- Forward declarations ---
void screen1_dot_task(void *param);
void screen2_label_task(void *param);
void screen3_timer_task(void *param);
void screen4_stamp_task(void *param);
void manage_tasks(void);
float get_gyro_x(void);
float get_gyro_y(void);
static lv_color_t gforce_to_color(float gx, float gy);
static String rtc_timestamp_string(); // helper to make timestamp for SD

// --- Utilities ---
float get_gyro_x(void) {
    // QMI8658 library getter, floating g value
    return QMI8658_getX();
}
float get_gyro_y(void) {
    return QMI8658_getY();
}
static lv_color_t gforce_to_color(float gx, float gy)
{
    float mag = sqrtf(gx*gx + gy*gy);
    float norm = constrain(mag / G_MAX, 0.0f, 1.0f);
    // green -> red scale
    uint8_t r = (uint8_t)(255 * norm);
    uint8_t g = (uint8_t)(255 * (1.0f - norm));
    return lv_color_make(r, g, 0);
}
static String rtc_timestamp_string()
{
    // Use rtc_time (PCF85063) if you update a global rtc_time in your RTC_Loop.
    // If you don't maintain a global struct, fallback to millis timestamp.
    // We'll attempt to use rtc_time if present (user earlier used 'rtc_time' struct).
    // If you have a getter, replace access accordingly.
    // Fallback to millis:
    char buf[64];
    // If you maintain rtc_time (datetime_t rtc_time), comment-in below and adjust
    // snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", rtc_time.year,... );
    snprintf(buf, sizeof(buf), "%lu", millis());
    return String(buf);
}

// --- Screen change hook: called when the active screen changes ---
// Wire this up to each screen with LV_EVENT_SCREEN_LOADED or SCREEN_CHANGED depending on SquareLine version
void lv_scr_change_hook(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_obj_t *act = lv_scr_act();

    // NOTE: adjust these names if your SquareLine screens are named differently
    screen1_active = (act == ui_Screen1 || act == ui_Screen1Container || act == ui_Screen1Screen);
    screen2_active = (act == ui_Screen2 || act == ui_Screen2Container);
    screen3_active = (act == ui_Screen3 || act == ui_Screen3Container);
    screen4_active = (act == ui_Screen4 || act == ui_Screen4Container);

    // Timer runs only when Timer screen is active
    timer_running = screen3_active;

    manage_tasks(); // suspend/resume the background tasks accordingly
}

// --- Lap button callback (Left button behavior per spec: stop timer and move to lap history) ---
void lap_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!screen3_active) return;

    // Stop the timer (per spec lbutton stops timer and moves current time to TimerScreen1)
    timer_running = false;

    // Save current timer value to history in slot 0 (most recent)
    lap_times[0] = timer_value;

    // Shift older
    for (int i = 3; i > 0; --i) lap_times[i] = lap_times[i-1]; // rotate older down

    // Update on-screen two prior laps (we show lap_times[0] and lap_times[1])
    char buf1[32], buf2[32];
    if (lap_times[0] > 0.0f) snprintf(buf1, sizeof(buf1), "Lap 1: %.2f s", lap_times[0]);
    else snprintf(buf1, sizeof(buf1), "Lap 1: --:--.--");
    if (lap_times[1] > 0.0f) snprintf(buf2, sizeof(buf2), "Lap 2: %.2f s", lap_times[1]);
    else snprintf(buf2, sizeof(buf2), "Lap 2: --:--.--");

    // Update UI labels asynchronously
    char *c1 = strdup(buf1);
    char *c2 = strdup(buf2);
    lv_async_call([](void *p){
        char **arr = (char**)p;
        lv_label_set_text(ui_TimeLabel1, arr[0]);
        lv_label_set_text(ui_TimeLabel2, arr[1]);
        free(arr[0]); free(arr[1]); free(arr);
    }, ({
        char **arr = (char**)malloc(sizeof(char*)*2);
        arr[0]=c1; arr[1]=c2; arr;
    }));
}

// --- Reset button callback (Right button per spec: reset Timerscreen to 00:00.000) ---
void reset_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    // Reset timer and clear displayed lap history (also reset peak labels)
    timer_running = false;
    timer_value = 0.0f;
    for (int i=0;i<4;i++) lap_times[i]=0.0f;

    // UI updates
    lv_async_call([](void *p){
        lv_label_set_text(ui_TimerLabel, INITIAL_TIMER_TEXT);
        // clear lap labels (assuming ui_LapLabels[0..3] exist)
        for (int i=0;i<4;i++){
            if (ui_LapLabels[i]) lv_label_set_text(ui_LapLabels[i], "--:--.--");
        }
        lv_label_set_text(ui_TimeLabel1, "Lap 1: --:--.--");
        lv_label_set_text(ui_TimeLabel2, "Lap 2: --:--.--");
        lv_label_set_text(ui_peakX_label, INITIAL_PEAK_TEXT);
        lv_label_set_text(ui_peakY_label, INITIAL_PEAK_TEXT);
        lv_label_set_text(ui_peakZ_label, INITIAL_PEAK_TEXT);
    }, NULL);
}

// --- Task manager: resume/suspend per-screen tasks ---
// Note: tasks are created and then immediately suspended after creation in setup()
void manage_tasks(void)
{
    // dot task -> screen1
    if (dotTaskHandle) {
        if (screen1_active) vTaskResume(dotTaskHandle); else vTaskSuspend(dotTaskHandle);
    }
    // label task -> screen2 (peak display)
    if (labelTaskHandle) {
        if (screen2_active) vTaskResume(labelTaskHandle); else vTaskSuspend(labelTaskHandle);
    }
    // timer task -> screen3 (timer & lap)
    if (timerTaskHandle) {
        if (screen3_active) vTaskResume(timerTaskHandle); else vTaskSuspend(timerTaskHandle);
    }
    // stamp task -> screen4 (stamping)
    if (stampTaskHandle) {
        if (screen4_active) vTaskResume(stampTaskHandle); else vTaskSuspend(stampTaskHandle);
    }
}

// --- Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("GForce Gauge Booting...");

    // Initialize drivers - keep exact order as your environment requires
    I2C_Init();
    Gyro_Init();
    RTC_Init();         // if you have a function to init rtc
    SD_Init();
    Lvgl_Display_Init();
    Touch_CST820_Init();

    // Init UI exported from SquareLine
    ui_init();

    // Hook screen change (use LV_EVENT_SCREEN_LOADED or SCREEN_LOADED, try SCREEN_CHANGED)
    // SquareLine/LVGL version differences: try LV_EVENT_SCREEN_LOADED or LV_EVENT_SCREEN_CHANGED
    lv_obj_add_event_cb(ui_Screen1, lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_Screen2, lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_Screen3, lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_Screen4, lv_scr_change_hook, LV_EVENT_SCREEN_LOADED, NULL);

    // Hook control buttons (names must match your SquareLine objects)
    lv_obj_add_event_cb(ui_LapButton, lap_button_cb, LV_EVENT_CLICKED, NULL);   // lbutton
    lv_obj_add_event_cb(ui_ResetButton, reset_button_cb, LV_EVENT_CLICKED, NULL); // rbutton

    // Prepare stamp container (you can also point to an existing SquareLine container if you made one)
    // We expect ui_Screen4Container to be the stamping area (SquareLine needs to provide this)
    stamp_container = ui_Screen4Container ? ui_Screen4Container : ui_Screen4; // fallback

    // Initial UI values
    lv_label_set_text(ui_TimerLabel, INITIAL_TIMER_TEXT);
    lv_label_set_text(ui_peakX_label, INITIAL_PEAK_TEXT);
    lv_label_set_text(ui_peakY_label, INITIAL_PEAK_TEXT);
    lv_label_set_text(ui_peakZ_label, INITIAL_PEAK_TEXT);
    for (int i=0;i<4;i++) {
        if (ui_LapLabels[i]) lv_label_set_text(ui_LapLabels[i], "--:--.--");
    }
    lv_label_set_text(ui_TimeLabel1, "Lap 1: --:--.--");
    lv_label_set_text(ui_TimeLabel2, "Lap 2: --:--.--");

    // Show splash (SquareLine splash screen name may vary)
    lv_scr_load(ui_SplashScreen);

    // auto transition to GForce (Screen1) after 2 s
    lv_timer_create([](lv_timer_t * t){
        lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
        lv_timer_del(t);
    }, 2000, NULL);

    // Create tasks: start suspended (create then suspend immediately)
    xTaskCreatePinnedToCore(screen1_dot_task,   "dot_task",   4096, NULL, 2, &dotTaskHandle,   1);
    if (dotTaskHandle) vTaskSuspend(dotTaskHandle);
    xTaskCreatePinnedToCore(screen2_label_task, "label_task", 4096, NULL, 2, &labelTaskHandle, 1);
    if (labelTaskHandle) vTaskSuspend(labelTaskHandle);
    xTaskCreatePinnedToCore(screen3_timer_task, "timer_task", 4096, NULL, 2, &timerTaskHandle, 1);
    if (timerTaskHandle) vTaskSuspend(timerTaskHandle);
    xTaskCreatePinnedToCore(screen4_stamp_task, "stamp_task", 4096, NULL, 2, &stampTaskHandle, 1);
    if (stampTaskHandle) vTaskSuspend(stampTaskHandle);

    // Load splash screen; user may swipe to screens or auto transition
    Serial.println("UI initialized — ready.");
}

// --- Main Loop ---
void loop() {
    lv_timer_handler(); // LVGL tick tasks
    delay(5);
}

/* -------------------------
   SCREEN TASKS
   ------------------------- */

// --- Screen1: moving dot task (±2.5 g mapped) ---
void screen1_dot_task(void *param)
{
    (void)param;
    const float alpha = 0.15f;
    static float fx = 0, fy = 0;

    while (1) {
        float gx = get_gyro_x();
        float gy = get_gyro_y();

        // update peaks
        if (fabs(gx) > PeakX) PeakX = fabs(gx);
        if (fabs(gy) > PeakY) PeakY = fabs(gy);
        // Z not used for center but record
        float gz = QMI8658_getZ();
        if (fabs(gz) > PeakZ) PeakZ = fabs(gz);

        // low-pass filter
        fx = fx * (1.0f - alpha) + gx * alpha;
        fy = fy * (1.0f - alpha) + gy * alpha;

        // map to pixel radius (±200 px area as your earlier draft; adapt if you want ±100)
        int16_t x = (int16_t)constrain((fx / G_MAX) * 200.0f, -200.0f, 200.0f);
        int16_t y = (int16_t)constrain((fy / G_MAX) * 200.0f, -200.0f, 200.0f);

        // compute color by magnitude
        lv_color_t color = gforce_to_color(gx, gy);

        // post UI update via async call
        int16_t *pos = new int16_t[2];
        pos[0] = x; pos[1] = y;
        lv_async_call([](void *p){
            int16_t *pp = (int16_t*)p;
            if (ui_DotImage) {
                lv_obj_set_pos(ui_DotImage, SCREEN_CENTER_X - DOT_RADIUS + pp[0], SCREEN_CENTER_Y - DOT_RADIUS + pp[1]);
                lv_obj_set_style_bg_color(ui_DotImage, gforce_to_color((float)pp[0]/200.0f*G_MAX, (float)pp[1]/200.0f*G_MAX), 0);
            }
            delete[] pp;
        }, pos);

        vTaskDelay(pdMS_TO_TICKS(DOT_UPDATE_MS));
    }
}

// --- Screen2: peak/neg/total labels (runs only when Peaks screen active) ---
void screen2_label_task(void *param)
{
    (void)param;
    float y_peak_pos = 0.0f;
    float y_peak_neg = 0.0f;
    float x_peak = 0.0f;

    while (1) {
        float gx = get_gyro_x();
        float gy = get_gyro_y();
        if (gy > y_peak_pos) y_peak_pos = gy;
        if (gy < y_peak_neg) y_peak_neg = gy;
        if (fabs(gx) > x_peak) x_peak = fabs(gx);

        char buf_px[32], buf_py[32], buf_pz[32];
        snprintf(buf_px, sizeof(buf_px), "Peak X: %.2f", x_peak);
        snprintf(buf_py, sizeof(buf_py), "Peak Y: %.2f", fabs(y_peak_pos) > fabs(y_peak_neg) ? fabs(y_peak_pos) : fabs(y_peak_neg));
        snprintf(buf_pz, sizeof(buf_pz), "Peak Z: %.2f", PeakZ);

        // Update UI asynchronously
        char *p1 = strdup(buf_px), *p2 = strdup(buf_py), *p3 = strdup(buf_pz);
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

// --- Screen3: Timer task (controls main timer on Timer screen) ---
void screen3_timer_task(void *param)
{
    (void)param;
    uint32_t last_ms = millis();

    while (1) {
        uint32_t now = millis();
        if (timer_running) {
            timer_value += (now - last_ms) / 1000.0f;
            // update Timer display
            char buf[32];
            // format mm:ss.hh (minutes:seconds.centiseconds)
            unsigned long total_cs = (unsigned long)(timer_value * 100.0f);
            unsigned int cs = total_cs % 100;
            unsigned long tot_s = total_cs / 100;
            unsigned int s = tot_s % 60;
            unsigned int m = (tot_s / 60) % 1000;
            snprintf(buf, sizeof(buf), "%02u:%02u.%02u", m, s, cs);
            char *txt = strdup(buf);
            lv_async_call([](void *p){
                char *s = (char*)p;
                if (ui_TimerLabel) lv_label_set_text(ui_TimerLabel, s);
                free(s);
            }, txt);
        } else {
            // if not running, ensure label shows 00:00.00 when timer_value == 0
            if (timer_value == 0.0f) {
                lv_async_call([](void *p){
                    if (ui_TimerLabel) lv_label_set_text(ui_TimerLabel, INITIAL_TIMER_TEXT);
                }, NULL);
            }
            last_ms = now;
        }
        last_ms = now;
        vTaskDelay(pdMS_TO_TICKS(TIMER_UPDATE_MS));
    }
}

// --- Screen4: stamp task (creates stamps, fades them, logs to SD) ---
void screen4_stamp_task(void *param)
{
    (void)param;

    while (1) {
        float gx = get_gyro_x();
        float gy = get_gyro_y();

        // clamp to ±G_MAX and map
        int16_t sx = (int16_t)constrain((gx / G_MAX) * 200.0f, -200.0f, 200.0f);
        int16_t sy = (int16_t)constrain((gy / G_MAX) * 200.0f, -200.0f, 200.0f);

        // color computed from current magnitude
        lv_color_t color = gforce_to_color(gx, gy);

        // Create stamp on UI and schedule fade + delete; also write to SD
        struct StampData {
            int16_t x, y;
            lv_color_t col;
            char *ts;
        };
        StampData *sd = (StampData*)malloc(sizeof(StampData));
        sd->x = sx; sd->y = sy;
        sd->col = color;
        String t = rtc_timestamp_string();
        sd->ts = strdup(t.c_str());

        lv_async_call([](void *p){
            StampData *s = (StampData*)p;
            // create an lv_obj (small circle) inside stamp_container
            lv_obj_t *st = lv_obj_create(stamp_container);
            lv_obj_set_size(st, 6, 6);
            lv_obj_set_style_radius(st, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(st, s->col, 0);
            lv_obj_set_style_border_width(st, 0, 0);
            lv_obj_set_style_opa(st, LV_OPA_COVER, 0);
            lv_obj_set_pos(st, SCREEN_CENTER_X - 3 + s->x, SCREEN_CENTER_Y - 3 + s->y);

            // Keep pointer for optional management (circular buffer)
            stamp_buf[stamp_buf_idx] = st;
            stamp_buf_idx = (stamp_buf_idx + 1) % MAX_STAMPS;

            // animate opacity -> transparent then delete
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, st);
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_time(&a, STAMP_FADE_MS);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_set_ready_cb(&a, [](lv_anim_t * a){
                lv_obj_del((lv_obj_t*)a->var);
            });
            lv_anim_start(&a);

            // Write to SD: CSV line timestamp, x,y
            char logbuf[128];
            // s->ts has timestamp (string); if it's just millis, fine
            snprintf(logbuf, sizeof(logbuf), "%s,%.3f,%.3f\n", s->ts ? s->ts : "0", (float)s->x/200.0f*G_MAX, (float)s->y/200.0f*G_MAX);
            SD_Write_String(logbuf);

            // cleanup stampdata
            free(s->ts);
            free(s);
        }, sd);

        vTaskDelay(pdMS_TO_TICKS(STAMP_UPDATE_MS));
    }
}
