#include "Display_ST7701.h"
#include <esp_lcd_panel_rgb.h>
#include <driver/ledc.h>

esp_lcd_panel_handle_t panel_handle = NULL;

// ------------------ LCD / RGB Init ------------------
void LCD_Init() {
    // Reset the panel (if needed, depends on board)
    Set_EXIO(EXIO_PIN1, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    Set_EXIO(EXIO_PIN1, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Configure RGB panel
    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = ESP_PANEL_LCD_RGB_TIMING_FREQ_HZ,
            .h_res = ESP_PANEL_LCD_HEIGHT,
            .v_res = ESP_PANEL_LCD_WIDTH,
            .hsync_pulse_width = ESP_PANEL_LCD_RGB_TIMING_HPW,
            .hsync_back_porch = ESP_PANEL_LCD_RGB_TIMING_HBP,
            .hsync_front_porch = ESP_PANEL_LCD_RGB_TIMING_HFP,
            .vsync_pulse_width = ESP_PANEL_LCD_RGB_TIMING_VPW,
            .vsync_back_porch = ESP_PANEL_LCD_RGB_TIMING_VBP,
            .vsync_front_porch = ESP_PANEL_LCD_RGB_TIMING_VFP,
            .flags = {
                .hsync_idle_low = 0,
                .vsync_idle_low = 0,
                .de_idle_high = 0,
                .pclk_active_neg = false,
                .pclk_idle_high = 0,
            },
        },
        .data_width = ESP_PANEL_LCD_RGB_DATA_WIDTH,
        .bits_per_pixel = ESP_PANEL_LCD_RGB_PIXEL_BITS,
        .num_fbs = ESP_PANEL_LCD_RGB_FRAME_BUF_NUM,
        .bounce_buffer_size_px = 10 * ESP_PANEL_LCD_HEIGHT,
        .psram_trans_align = 64,
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
            .disp_active_low = 0,
            .refresh_on_demand = 0,
            .fb_in_psram = true,
            .double_fb = true,
        },
    };

    esp_lcd_new_rgb_panel(&rgb_config, &panel_handle);
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

    // Initialize touch and backlight
    Touch_Init();
    Backlight_Init();
}

// ------------------ LVGL flush ------------------
void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t* color) {
    Xend = Xend + 1;
    Yend = Yend + 1;
    if (Xend >= ESP_PANEL_LCD_WIDTH) Xend = ESP_PANEL_LCD_WIDTH;
    if (Yend >= ESP_PANEL_LCD_HEIGHT) Yend = ESP_PANEL_LCD_HEIGHT;

    esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, (uint8_t*)color);
}

// ------------------ Backlight ------------------
uint8_t LCD_Backlight = 50;

void Backlight_Init() {
    ledcAttach(LCD_Backlight_PIN, Frequency, Resolution);
    Set_Backlight(LCD_Backlight);
}

void Set_Backlight(uint8_t light) {
    if (light > Backlight_MAX) light = Backlight_MAX;
    uint32_t duty = light * 10;
    if (duty == 1000) duty = 1024;
    ledcWrite(LCD_Backlight_PIN, duty);
}
