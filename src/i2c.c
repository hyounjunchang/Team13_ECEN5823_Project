/***********************************************************************
 * @file      i2c.c
 * @brief     I2C module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 4, 2025
 *
 * @resources Lecture 6 Slides
 *
 *
 *
 */

#include "i2c.h"
#include <sl_i2cspm.h>
#include <stdint.h>
#include "timer.h"
#include "gpio.h"

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#define I2C_READ 0b1
#define I2C_WRITE 0b0

// For SI7021 Temperature Sensor
#define SI7021_I2C_PORT gpioPortC
#define SI7021_SCL_PIN 10 // I2C0_SCL_14
#define SI7021_SDA_PIN 11 // I2C0_SDA_16

#define SI7021_ENABLE_PORT gpioPortD
#define SI7021_ENABLE_PIN 15

// SI7021 Address
#define SI7021_ADDR 0b1000000

// SI7021 Commands
#define MEASURE_TEMP_NO_HOLD 0xF3// no hold master mode, no clock stretching


// Used from Lecture 6 slides
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
}

// Used from Lecture 6 slides
int SI7021_get_temperature(){
  gpioPowerOn_SI7021();
  // wait for power-up time (max 80ms)
  timerWaitUs(80000);

  SI7021_start_measure_temp();
  // wait for temperature reading, max 10.8ms for 14-bit
  timerWaitUs(11000); // wait at least 11ms

  uint16_t sensor_value = SI7021_read_measured_temp();
  float sensor_temp = SI7021_convert_temp(sensor_value);
  int rounded_sensor_temp = (int)sensor_temp;
  LOG_INFO("Read temperature %iC\r\n", rounded_sensor_temp);

  gpioPowerOff_SI7021();
  return sensor_temp;
}

// returns temperature in Celsius
float SI7021_convert_temp(uint16_t temp_code){
  return (175.72 * temp_code) / 65536 - 46.85;
}

// Used from Lecture 6 slides
void SI7021_start_measure_temp(){
  // Send Measure Temperature command
  I2C_TransferReturn_TypeDef transferStatus; // make this global for IRQs in A4
  I2C_TransferSeq_TypeDef transferSequence; // this one can be local
  uint8_t cmd_data; // make this global for IRQs in A4

  // write command to sensor to read temperature data
  cmd_data = MEASURE_TEMP_NO_HOLD;
  transferSequence.addr = SI7021_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE;
  transferSequence.buf[0].data = &cmd_data; // pointer to data to write
  transferSequence.buf[0].len = sizeof(cmd_data);
  transferStatus = I2CSPM_Transfer (I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
      LOG_ERROR("I2C transfer Failed\r\n");
  }

}

uint16_t SI7021_read_measured_temp(){
  // send Read command
  I2C_TransferReturn_TypeDef transferStatus; // make this global for IRQs in A4
  I2C_TransferSeq_TypeDef transferSequence; // this one can be local

  // 2-byte data received
  uint8_t read_data[2]; // make this global for IRQs in A4

  transferSequence.addr = SI7021_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_READ;
  transferSequence.buf[0].data = read_data; // pointer to data to write
  transferSequence.buf[0].len = sizeof(read_data);
  transferStatus = I2CSPM_Transfer (I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
      LOG_ERROR("I2C transfer Failed\r\n");
  }

  // SI7021 sends 2-byte data, MSB then LSB (big-endian)
  // uint16_t is little-endian, so you can't directly store to uint16_t
  // add values from 0
  uint16_t sensor_value = read_data[1];
  sensor_value += (uint16_t)read_data[0] << 8;

  return sensor_value;
}
