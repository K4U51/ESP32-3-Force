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

    // Create a background task for drivers
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
    if (!ui_dot) return;  // make sure UI elements exist

    // Center (240, 240) for 480x480 screen
    // Scale ±1G = ±150 pixels
    float xpos = 240 - ((x / 9.81f) * 150);
    float ypos = 240 - ((y / 9.81f) * 150);

    // Clamp for safety
    xpos = constrain(xpos, 20, 460);
    ypos = constrain(ypos, 20, 460);

    lv_obj_set_pos(ui_dot, (int)xpos, (int)ypos);

    // Update G-force readouts
    if (ui_Accel) lv_label_set_text_fmt(ui_Accel, "%.2f", max(y / 9.81f, 0.0f));
    if (ui_Brake) lv_label_set_text_fmt(ui_Brake, "%.2f", abs(min(y / 9.81f, 0.0f)));
    if (ui_Left)  lv_label_set_text_fmt(ui_Left,  "%.2f", abs(min(x / 9.81f, 0.0f)));
    if (ui_Right) lv_label_set_text_fmt(ui_Right, "%.2f", max(x / 9.81f, 0.0f));
}

// ------------------ Setup ------------------
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== System Booting ===");

    // 1️⃣ Initialize all low-level drivers
    Driver_Init();   // I2C, EXIO, sensors, battery

    // 2️⃣ Initialize LCD hardware
    LCD_Init();      // Sets up ST7701 RGB panel + panel_handle

    // 3️⃣ Initialize LVGL and link to display driver
    Lvgl_Init();     // Creates LVGL buffers and flush callback

    // 4️⃣ Initialize the SquareLine-generated UI
    Serial.println("Initializing UI...");
    ui_init();

    // 5️⃣ Optional confirmation label
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "GForce Gauge Ready!");
    lv_obj_center(label);

    Serial.println("=== Setup Complete ===");
}

// ------------------ Main Loop ------------------
void loop()
{
    Lvgl_GForce_Loop();  // Update moving dot + G values
    Lvgl_Loop();         // Keep LVGL alive
    delay(5);
}
