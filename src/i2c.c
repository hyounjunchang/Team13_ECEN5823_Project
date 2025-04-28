/***********************************************************************
 * @file      i2c.c
 * @brief     I2C module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Apr 24, 2025
 *
 * @resources Lecture 6 Slides, Lecture 8 Slides
 *
 *
 *
 */

#include "i2c.h"
#include <sl_i2cspm.h>
#include <stdint.h>
#include "timer.h"
#include "gpio.h"
#include "app.h"

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#if DEVICE_IS_BLE_SERVER == 1 // BLE Server Macros Declarations
#define I2C_READ 0b1
#define I2C_WRITE 0b0

// For SI7021 Temperature Sensor
// SCL and SDA port same for VEML6030
#define SI7021_I2C_PORT gpioPortC
#define SI7021_SCL_PIN 10 // I2C0_SCL_14
#define SI7021_SDA_PIN 11 // I2C0_SDA_16

#define SI7021_ENABLE_PORT gpioPortD
#define SI7021_ENABLE_PIN 15

// SI7021 Address
#define SI7021_ADDR 0b1000000

// SI7021 Commands
#define MEASURE_TEMP_NO_HOLD 0xF3// no hold master mode, no clock stretching

// VEML6030 Address
#define VEML6030_ADDR 0x48 // 0x48 default (ADDR = 1), 0x10 (ADDR = 0)

// VEML6030 Commands
#define VEML6030_ALS_CONF_0 0x00
#define VEML6030_POWER_SAVING 0x03
#define VEML6030_ALS 0x04

// global variables
I2C_TransferSeq_TypeDef transferSequence; // global

// for VEML6030
uint8_t VEML6030_cmd_data[3];
uint8_t VEML6030_read_data[2];


// Used from Lecture 6 slides
// Using I2CSPM_Init, as it's stated it's okay per spec
void initialize_I2C(){
  // Initialize the I2C hardware
  I2CSPM_Init_TypeDef I2C_Config = {
  .port = I2C0,
  .sclPort = SI7021_I2C_PORT,
  .sclPin = SI7021_SCL_PIN,
  .sdaPort = SI7021_I2C_PORT,
  .sdaPin = SI7021_SDA_PIN,
  .portLocationScl = 14,
  .portLocationSda = 16,
  .i2cRefFreq = 0,
  .i2cMaxFreq = I2C_FREQ_STANDARD_MAX,
  .i2cClhr = i2cClockHLRStandard
  };
  I2CSPM_Init(&I2C_Config);
  // uint32_t i2c_bus_frequency = I2C_BusFreqGet (I2C0);

  // Initialize ambient light sensor
  VEML6030_initialize();
}

// Using I2CSPM for initialization
void VEML6030_initialize(){
  // Operation Mode - VDD = 3.3 V, PSM = 11, refresh time 4100 ms, ALS_GAIN = “01” for minimum gain
  // For VEML6030, 3 byte data is sent in cmd-data_LSB-data_MSB
  I2C_TransferReturn_TypeDef transferStatus;

  // Set mode
  VEML6030_cmd_data[0] = VEML6030_ALS_CONF_0;
  VEML6030_cmd_data[1] = 0b00000000; // ALS_PERS = 00, Protect # = 1, ALS_INT_EN = 0, ALS_SD = 0 (ALS power on)
  VEML6030_cmd_data[2] = 0b00001000; // ALS_GAIN = 01, ALS_IT = 0000 for 100ms integration time

  transferSequence.addr = VEML6030_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE;
  transferSequence.buf[0].data = &(VEML6030_cmd_data[0]); // pointer to data to write
  transferSequence.buf[0].len = 3;

  // send twice, current hardware sometimes fails first I2C transfer
  transferStatus = I2CSPM_Transfer(I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
      LOG_ERROR("I2C transfer Failed\r\n");
  }
  transferStatus = I2CSPM_Transfer(I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
      LOG_ERROR("I2C transfer Failed\r\n");
  }

  // Power Saving Mode
  VEML6030_cmd_data[0] = VEML6030_POWER_SAVING;
  VEML6030_cmd_data[1] = 0b111; // PSM = 11, PSM_EN = 1
  VEML6030_cmd_data[2] = 0b0;

  transferSequence.addr = VEML6030_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE;
  transferSequence.buf[0].data = &(VEML6030_cmd_data[0]); // pointer to data to write
  transferSequence.buf[0].len = 3;

  transferStatus = I2CSPM_Transfer(I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
     LOG_ERROR("I2C transfer Failed\r\n");
  }
}

// since refresh time = 4100ms, call this every >5 sec
void VEML6030_start_read_ambient_light_level(){
  // Set mode
  VEML6030_cmd_data[0] = VEML6030_ALS;

  transferSequence.addr = VEML6030_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE_READ;
  transferSequence.buf[0].data = &(VEML6030_cmd_data[0]); // write cmd = register
  transferSequence.buf[0].len = 1;
  transferSequence.buf[1].data = &(VEML6030_read_data[0]);
  transferSequence.buf[1].len = 2;

  I2C_TransferReturn_TypeDef transferStatus;
  // receving SW fault if NVIC interrupt for I2C_Transfer is used, until this is fixed I2CSPM useds
  transferStatus = I2CSPM_Transfer(I2C0, &transferSequence);
  if (transferStatus < 0) {
    LOG_ERROR("%d\r\n", transferStatus);
  }
}

// converts ADC value to light measurement in lux
float VEML6030_convert_to_lux(uint16_t adc_val){
  // ALS_GAIN = 01, PSM = 11, ALS_IT = 0000
  return 0.0288 * adc_val;
}

float VEML6030_read_measured_ambient_light(){
  // sends 2-byte data, MSB then LSB (big-endian)
  // uint16_t is little-endian, so you can't directly store to uint16_t
  // add values from 0
  uint16_t sensor_value = VEML6030_read_data[0];
  sensor_value += (uint16_t)VEML6030_read_data[1] << 8;
  return VEML6030_convert_to_lux(sensor_value);
}

#endif
