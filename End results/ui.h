#ifndef _UI_H
#define _UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

//------------------------------------------------------------
// Screen Objects
//------------------------------------------------------------
extern lv_obj_t *ui_SplashScreen;
extern lv_obj_t *ui_GForceScreen;
extern lv_obj_t *ui_PeaksScreen;
extern lv_obj_t *ui_TimerScreen;
extern lv_obj_t *ui_StampScreen;

//------------------------------------------------------------
// G-Force Screen Objects
//------------------------------------------------------------
extern lv_obj_t *ui_gforce_dot;       // Moving G-Force dot (centered)
extern lv_obj_t *ui_bg_image;         // Optional background image

//------------------------------------------------------------
// Peaks Screen Objects
//------------------------------------------------------------
extern lv_obj_t *ui_peakX_label;
extern lv_obj_t *ui_peakY_label;
extern lv_obj_t *ui_peakZ_label;

//------------------------------------------------------------
// Timer Screen Objects
//------------------------------------------------------------
extern lv_obj_t *ui_timer_label;
extern lv_obj_t *ui_reset_button;

//------------------------------------------------------------
// Lap Screen / Labels (Timer screen or separate container)
//------------------------------------------------------------
extern lv_obj_t *ui_LapButton;        // For lap save
extern lv_obj_t *ui_LapLabels[4];     // Array of label refs (Lap 1–4)
extern lv_obj_t *ui_TimeLabel1;
extern lv_obj_t *ui_TimeLabel2;
extern lv_obj_t *ui_TimerLabel;

//------------------------------------------------------------
// Stamp Screen Objects (for trail dots)
//------------------------------------------------------------
extern lv_obj_t *ui_StampContainer;   // Container to hold fading dots

//------------------------------------------------------------
// LVGL Init Function (from SquareLine export)
//------------------------------------------------------------
void ui_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _UI_H
