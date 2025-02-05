/***********************************************************************
 * @file      i2c.c
 * @brief     I2C module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 4, 2025
 *
 * @resources
 *
 *
 * Editor: Feb 4, 2025 Hyounjun Chang
 * Change: Initial I2CSPM used
 *
 */

#include "i2c.h"
#include <sl_i2cspm.h>
#include <stdint.h>

// For Si7021 Temperature Sensor
#define Si7021_port gpioPortC
#define Si7021_SCL_pin 10 // I2C0_SCL_14
#define Si7021_SDA_pin 11 // I2C0_SDA_16


void initialize_I2C(){
  // Initialize the I2C hardware
  I2CSPM_Init_TypeDef I2C_Config = {
  .port = I2C0,
  .sclPort = Si7021_port,
  .sclPin = Si7021_SCL_pin,
  .sdaPort = Si7021_port,
  .sdaPin = Si7021_SDA_pin,
  .portLocationScl = 14,
  .portLocationSda = 16,
  .i2cRefFreq = 0,
  .i2cMaxFreq = I2C_FREQ_STANDARD_MAX,
  .i2cClhr = i2cClockHLRStandard
  };
  I2CSPM_Init(&I2C_Config);
  uint32_t i2c_bus_frequency = I2C_BusFreqGet (I2C0);
}
