#include "LVGL_Driver.h"

lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;

// ------------------ LVGL → ST7701 Display Flush ------------------
void Lvgl_Display_LCD(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    // Cast to uint16_t* because LVGL color is 16-bit per pixel
    LCD_addWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
    lv_disp_flush_ready(disp_drv);
}

// ------------------ Touchpad Read ------------------
void Lvgl_Touchpad_Read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    Touch_Read_Data();
    if (touch_data.points != 0) {
        data->point.x = touch_data.x;
        data->point.y = touch_data.y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    // Reset touch data for next read
    touch_data.x = 0;
    touch_data.y = 0;
    touch_data.points = 0;
    touch_data.gesture = NONE;
}

// ------------------ LVGL Tick ------------------
void example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// ------------------ LVGL Initialization ------------------
void Lvgl_Init(void) {
    lv_init();

    // Allocate PSRAM buffers for full-screen double buffering
    buf1 = (lv_color_t*) heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*) heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        Serial.println("ERROR: LVGL PSRAM allocation");
        while (1);
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_WIDTH * LCD_HEIGHT);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = Lvgl_Display_LCD;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = Lvgl_Touchpad_Read;
    lv_indev_drv_register(&indev_drv);

    // Start LVGL tick timer
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);
}

// ------------------ LVGL Loop ------------------
void Lvgl_Loop(void) {
    lv_timer_handler();
}
