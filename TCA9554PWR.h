#pragma once

#include <Arduino.h>
#include "I2C_Driver.h"

// ------------------ EXIO Pin Definitions ------------------
#define EXIO_PIN1  1
#define EXIO_PIN2  2
#define EXIO_PIN3  3
#define EXIO_PIN4  4
#define EXIO_PIN5  5
#define EXIO_PIN6  6
#define EXIO_PIN7  7
#define EXIO_PIN8  8

// ------------------ TCA9554PWR Registers & Macros ------------------
#define TCA9554_ADDRESS      0x20
#define TCA9554_INPUT_REG    0x00
#define TCA9554_OUTPUT_REG   0x01
#define TCA9554_Polarity_REG 0x02
#define TCA9554_CONFIG_REG   0x03

#define Low   0
#define High  1

// ------------------ Function Declarations ------------------

// Operation register
uint8_t I2C_Read_EXIO(uint8_t REG);                         
uint8_t I2C_Write_EXIO(uint8_t REG,uint8_t Data);           

// Set EXIO mode
void Mode_EXIO(uint8_t Pin,uint8_t State);                  
void Mode_EXIOS(uint8_t PinState);                          

// Read EXIO status
uint8_t Read_EXIO(uint8_t Pin);                             
uint8_t Read_EXIOS(uint8_t REG = TCA9554_INPUT_REG);        

// Set the EXIO output status
void Set_EXIO(uint8_t Pin,uint8_t State);                   
void Set_EXIOS(uint8_t PinState);                           

// Flip EXIO state
void Set_Toggle(uint8_t Pin);                               

// Initialize TCA9554PWR
void TCA9554PWR_Init(uint8_t PinState = 0x00);             
