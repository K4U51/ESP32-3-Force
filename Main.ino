#include <Arduino.h>
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "Gyro_QMI8658.h"
#include "BAT_Driver.h"
#include "Display_ST7701.h"
#include "Touch_CST820.h"
#include "LVGL_Driver.h"
#include "ui.h"  // SquareLine generated UI

// ------------------ Global Variables ------------------
float x = 0, y = 0, z = 0;  // Accelerometer

// ------------------ Driver Task ------------------
void Driver_Loop(void *parameter)
{
    while (1)
    {
        QMI8658_Loop();  // Updates Accel internally
        BAT_Get_Volts();

        // Copy latest accelerometer values
        x = Accel.x;
        y = Accel.y;
        z = Accel.z;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ------------------ Driver Init ------------------
void Driver_Init()
{
    Serial.println("Initializing Drivers...");

    I2C_Init();
    TCA9554PWR_Init(0x00);

    // Enable LCD power/backlight
    Set_EXIO(EXIO_PIN8, Low);
    delay(50);
    Set_Backlight(100);

    QMI8658_Init();
    BAT_Init();

    xTaskCreatePinnedToCore(
        Driver_Loop,
        "Driver Task",
        4096,
        NULL,
        3,
        NULL,
        0
    );
}

// ------------------ G-Force Screen Update ------------------
void Lvgl_GForce_Loop()
{
    if (!ui_dot) return;

    // Center is 240, 240 for 480x480 display
    // Scale: ±1G = ±150 pixels from center
    float xpos = 240 - ((x / 9.81f) * 150);
    float ypos = 240 - ((y / 9.81f) * 150);

    // Clamp to screen bounds (with margin for dot size ~37px)
    xpos = constrain(xpos, 20, 460);
    ypos = constrain(ypos, 20, 460);

    lv_obj_set_pos(ui_dot, (int)xpos, (int)ypos);

    // Update labels with G-force values
    if (ui_Accel) {
        float accel_g = y / 9.81f;
        lv_label_set_text_fmt(ui_Accel, "%.2f", max(accel_g, 0.0f));
    }
    if (ui_Brake) {
        float brake_g = y / 9.81f;
        lv_label_set_text_fmt(ui_Brake, "%.2f", abs(min(brake_g, 0.0f)));
    }
    if (ui_Left) {
        float left_g = x / 9.81f;
        lv_label_set_text_fmt(ui_Left, "%.2f", abs(min(left_g, 0.0f)));
    }
    if (ui_Right) {
        float right_g = x / 9.81f;
        lv_label_set_text_fmt(ui_Right, "%.2f", max(right_g, 0.0f));
    }
}

// ------------------ Setup ------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== System Booting ===");

    // ----------------------------
    // 1️⃣ Initialize drivers first
    // ----------------------------
    Driver_Init();   // Sensors, I2C, gyro, battery, etc.

    // ----------------------------
    // 2️⃣ Initialize hardware
    // ----------------------------
    LCD_Init();      // Sets up ST7701 panel, backlight, touch
                     // Note: no LVGL registration here, just hardware

    // ----------------------------
    // 3️⃣ Initialize LVGL
    // ----------------------------
    Lvgl_Init();     // lv_init() + flush callback + draw buffers
                     // Uses the panel_handle created in LCD_Init()

    // ----------------------------
    // 4️⃣ Initialize your SquareLine UI
    // ----------------------------
    Serial.println("Initializing UI...");
    ui_init();       // Creates screens, labels, dot, etc.

    // Optional: quick test to confirm LVGL works
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "GForce Gauge Ready!");
    lv_obj_center(label);

    Serial.println("=== Setup Complete ===");
}

// ------------------ Main Loop ------------------
void loop()
{
    Lvgl_GForce_Loop();  // Update G-force display
    Lvgl_Loop();         // LVGL internal handler (lv_timer_handler)
    delay(5);
}
