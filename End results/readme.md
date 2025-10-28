```
GForceGauge/
│
├─ main.ino                # Main Arduino sketch (the full LVGL code)
├─ lvgl/                   # LVGL library folder (can be installed via Library Manager)
│
├─ images/                 # Your PNG images converted to lv_img_dsc_t
│   ├─ splash_img.c
│   ├─ screen1_img.c
│   ├─ screen2_img.c
│   ├─ screen3_img.c
│   ├─ screen4_img.c
│   ├─ dot_img.c
│   └─ stamp_img.c
│
├─ include/                # Header files for drivers and images
│   ├─ images.h
│   └─ gyro_driver.h       # Optional: your gyro/accelerometer interface
│
├─ drivers/                # Your ESP32-S3 hardware drivers
│   ├─ display_driver.h/.cpp
│   ├─ touch_driver.h/.cpp
│   └─ gyro_driver.h/.cpp
│
├─ lib/                    # Optional extra libraries not installed via Library Manager
│
└─ platformio.ini / Arduino project metadata
