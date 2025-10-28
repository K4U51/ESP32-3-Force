#include "LVGL_Driver.h"
#include "Display_ST7701.h"
#include "Touch_CST820.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
lv_disp_drv_t disp_drv;

static void lv_tick_task(void *arg) {
    lv_tick_inc(5);
}

void Lvgl_Display_LCD(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (panel_handle)
        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    lv_disp_flush_ready(drv);
}

void Lvgl_Touchpad_Read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    Touch_Read_Data();
    if (touch_data.points > 0) {
        data->point.x = touch_data.x;
        data->point.y = touch_data.y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void Lvgl_Init(void) {
    Serial.println("Initializing LVGL...");
    lv_init();

    const uint32_t buf_lines = 40;
    size_t buf_size = 480 * buf_lines * sizeof(lv_color_t);
    buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 480 * buf_lines);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 480;
    disp_drv.flush_cb = Lvgl_Display_LCD;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = Lvgl_Touchpad_Read;
    lv_indev_drv_register(&indev_drv);

    esp_timer_handle_t lvgl_tick_timer;
    const esp_timer_create_args_t tick_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"
    };
    esp_timer_create(&tick_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 5000);

    lv_obj_t *scr = lv_disp_get_scr_act(lv_disp_get_default());
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    Serial.println("LVGL initialized successfully!");
}

void Lvgl_Loop(void) {
    lv_timer_handler();
}
