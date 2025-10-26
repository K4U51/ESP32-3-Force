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

    // Removed Flash_test() because undefined

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

    float xpos = 240 + ((x / 9.81f) * 150);
    float ypos = 240 + ((y / 9.81f) * 150);

    xpos = constrain(xpos, 0, 479);
    ypos = constrain(ypos, 0, 479);

    lv_obj_set_pos(ui_dot, (int)xpos, (int)ypos);

    if (ui_Accel) lv_label_set_text_fmt(ui_Accel, "Accel: %.2f", max(y / 9.81f, 0.0f));
    if (ui_Brake) lv_label_set_text_fmt(ui_Brake, "Brake: %.2f", abs(min(y / 9.81f, 0.0f)));
    if (ui_Left) lv_label_set_text_fmt(ui_Left, "Left: %.2f", abs(min(x / 9.81f, 0.0f)));
    if (ui_Right) lv_label_set_text_fmt(ui_Right, "Right: %.2f", max(x / 9.81f, 0.0f));
}

// ------------------ Setup ------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== System Booting ===");

    // Initialize drivers and sensors
    Driver_Init();

    // Display init (includes touch/backlight)
    LCD_Init();  

    // LVGL init + PSRAM buffers
    Lvgl_Init();

    // Remove demo label if Lvgl_Init created one
    lv_obj_clean(lv_scr_act());

    // Initialize SquareLine UI
    ui_init();
    lv_scr_load(ui_gforce);

    // Removed SD_Init() call if undefined

    Serial.println("=== Setup Complete ===");
}

// ------------------ Main Loop ------------------
void loop()
{
    Lvgl_GForce_Loop();
    Lvgl_Loop(); // LVGL internal handler
    delay(5);
}
