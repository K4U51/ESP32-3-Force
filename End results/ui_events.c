#include "ui.h"

// Splash screen gesture handler
void ui_event_Splash(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT) {
            _ui_screen_change(&ui_Stats, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, &ui_Stats_screen_init);
        }
    }
}

// Stats screen gesture handler
void ui_event_Stats(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT) {
            _ui_screen_change(&ui_Gauge, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, &ui_Gauge_screen_init);
        } else if (dir == LV_DIR_RIGHT) {
            _ui_screen_change(&ui_Splash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, &ui_Splash_screen_init);
        }
    }
}

// Gauge screen gesture handler
void ui_event_Gauge(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            _ui_screen_change(&ui_Stats, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, &ui_Stats_screen_init);
        }
    }
}

// Reset button callback
void reset_peaks_cb(lv_event_t * e)
{
    // Reference to your peak variables in main
    extern float peak_accel, peak_brake, peak_left, peak_right;
    
    peak_accel = 0;
    peak_brake = 0;
    peak_left = 0;
    peak_right = 0;
}
