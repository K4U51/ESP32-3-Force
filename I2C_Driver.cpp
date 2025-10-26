#include "I2C_Driver.h"
#include "Arduino.h"
#include <Wire.h>

// Define pins if not already done elsewhere
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 8
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 9
#endif

void I2C_Init(void) {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000); // 100 kHz for stability
    Wire.setTimeOut(50); // 50ms timeout
    Serial.printf("I2C initialized: SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
}

bool I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
    // Step 1: tell device which register to read from
    Wire.beginTransmission(Driver_addr);
    Wire.write(Reg_addr);
    if (Wire.endTransmission(false) != 0) {  // false = repeated start
        Serial.printf("I2C Read: no ACK from device 0x%02X\n", Driver_addr);
        return false;
    }

    // Step 2: read the data
    uint8_t bytesRead = Wire.requestFrom(Driver_addr, (uint8_t)Length, (uint8_t)true);
    if (bytesRead != Length) {
        Serial.printf("I2C read underrun: expected %lu, got %u from 0x%02X\n",
                      Length, bytesRead, Driver_addr);
        return false;
    }

    for (uint32_t i = 0; i < bytesRead; i++) {
        Reg_data[i] = Wire.read();
    }
    return true;
}

bool I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
    Wire.beginTransmission(Driver_addr);
    Wire.write(Reg_addr);

    for (uint32_t i = 0; i < Length; i++) {
        Wire.write(Reg_data[i]);
    }

    if (Wire.endTransmission(true) != 0) {  // true = STOP after write
        Serial.printf("I2C Write failed: 0x%02X\n", Driver_addr);
        return false;
    }
    return true;
}
