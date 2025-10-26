#include "TCA9554PWR.h"
#include <Arduino.h>

// ------------------ Stub I2C Read ------------------
uint8_t I2C_Read_EXIO(uint8_t REG)
{
    // Return 0 by default, nothing is connected
    return 0;
}

// ------------------ Stub I2C Write ------------------
uint8_t I2C_Write_EXIO(uint8_t REG, uint8_t Data)
{
    // Do nothing, return 0 (success)
    return 0;
}

// ------------------ Set EXIO Mode ------------------
void Mode_EXIO(uint8_t Pin, uint8_t State)
{
    // Do nothing
}

void Mode_EXIOS(uint8_t PinState)
{
    // Do nothing
}

// ------------------ Read EXIO Status ------------------
uint8_t Read_EXIO(uint8_t Pin)
{
    return 0;
}

uint8_t Read_EXIOS(uint8_t REG)
{
    return 0;
}

// ------------------ Set EXIO Output ------------------
void Set_EXIO(uint8_t Pin, uint8_t State)
{
    // Do nothing
}

void Set_EXIOS(uint8_t PinState)
{
    // Do nothing
}

// ------------------ Flip EXIO State ------------------
void Set_Toggle(uint8_t Pin)
{
    // Do nothing
}

// ------------------ Initialize TCA9554PWR ------------------
void TCA9554PWR_Init(uint8_t PinState)
{
    // Do nothing
}
