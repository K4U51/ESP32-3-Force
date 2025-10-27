#include "Display_ST7701.h"
#include "LVGL_Driver.h"
#include <esp_lcd_panel_rgb.h>
#include <driver/ledc.h>

esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
static lv_disp_drv_t disp_drv;

void LCD_Init() {
    Serial.println("LCD_Init: Starting ST7701 RGB setup...");

    // Reset panel via expander (if needed)
    Set_EXIO(EXIO_PIN1, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    Set_EXIO(EXIO_PIN1, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,  // 16 MHz safe default
            .h_res = 480,
            .v_res = 480,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 20,
            .hsync_front_porch = 10,
            .vsync_pulse_width = 10,
            .vsync_back_porch = 20,
            .vsync_front_porch = 10,
            .flags = {
                .pclk_active_neg = false,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .hsync_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_HSYNC,
        .vsync_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_VSYNC,
        .de_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_DE,
        .pclk_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_PCLK,
        .disp_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_DISP,
        .data_gpio_nums = {
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA0, ESP_PANEL_LCD_PIN_NUM_RGB_DATA1,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA2, ESP_PANEL_LCD_PIN_NUM_RGB_DATA3,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA4, ESP_PANEL_LCD_PIN_NUM_RGB_DATA5,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA6, ESP_PANEL_LCD_PIN_NUM_RGB_DATA7,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA8, ESP_PANEL_LCD_PIN_NUM_RGB_DATA9,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA10, ESP_PANEL_LCD_PIN_NUM_RGB_DATA11,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA12, ESP_PANEL_LCD_PIN_NUM_RGB_DATA13,
            ESP_PANEL_LCD_PIN_NUM_RGB_DATA14, ESP_PANEL_LCD_PIN_NUM_RGB_DATA15,
        },
        .flags = {
            .fb_in_psram = true,
            .double_fb = true,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // ✅ Bind LVGL to this RGB panel
    buf1 = (lv_color_t *)heap_caps_malloc(480 * 20 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(480 * 20 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 480 * 20);

    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
        lv_disp_flush_ready(drv);
    };
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 480;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("LCD_Init: LVGL display registered.");

    // Backlight
    ledcSetup(0, 5000, 10);
    ledcAttachPin(LCD_Backlight_PIN, 0);
    ledcWrite(0, 512);  // 50% brightness

    Serial.println("LCD_Init: Backlight ON.");

    Touch_Init();
}
