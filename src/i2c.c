/***********************************************************************
 * @file      i2c.c
 * @brief     I2C module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 13, 2025
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

static uint8_t curr_i2c_target = I2C_TRANSFER_NONE;

// global variables
I2C_TransferSeq_TypeDef transferSequence; // global

// for SI7021
uint8_t cmd_data; // make this global for IRQs in A4
uint8_t read_data[2]; // make this global for IRQs in A4

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

void set_I2C_transfer_target(i2c_transfer_target target){
  curr_i2c_target = target;
}

void clear_I2C_transfer_target(){
  curr_i2c_target = I2C_TRANSFER_NONE;
}

i2c_transfer_target get_I2C_transfer_target(){
  return curr_i2c_target;
}


// returns temperature in Celsius
float SI7021_convert_temp(uint16_t temp_code){
  return (175.72 * temp_code) / 65536 - 46.85;
}

// Used from Lecture 6 slides
void SI7021_start_measure_temp(){
  set_I2C_transfer_target(I2C_TRANSFER_SI7021);

  // Sleep at EM1 during I2C
  if (LOWEST_ENERGY_MODE == 2){
     sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM2);
     sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  if (LOWEST_ENERGY_MODE == 3){
     sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }

  // write command to sensor to read temperature data
  cmd_data = MEASURE_TEMP_NO_HOLD;
  transferSequence.addr = SI7021_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE;
  transferSequence.buf[0].data = &cmd_data; // pointer to data to write
  transferSequence.buf[0].len = sizeof(cmd_data);

  I2C_TransferReturn_TypeDef transferStatus;
  transferStatus = I2C_TransferInit(I2C0, &transferSequence);
  if (transferStatus < 0) {
    LOG_ERROR("%d\r\n", transferStatus);
  }
}

void SI7021_wait_temp_sensor(){
  timerWaitUs_irq(11000); // wait 11ms for sensor data
}

void SI7021_start_read_sensor(){
  set_I2C_transfer_target(I2C_TRANSFER_SI7021);

  // Sleep at EM1 during I2C
  if (LOWEST_ENERGY_MODE == 2){
     sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM2);
     sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  if (LOWEST_ENERGY_MODE == 3){
     sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }

  // send Read command
  transferSequence.addr = SI7021_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_READ;
  transferSequence.buf[0].data = getReadData_buf(); // pointer to data to write
  transferSequence.buf[0].len = 2;

  I2C_TransferReturn_TypeDef transferStatus;
  transferStatus = I2C_TransferInit(I2C0, &transferSequence);
  if (transferStatus < 0) {
        LOG_ERROR("%d\r\n", transferStatus);
  }
}

int SI7021_read_measured_temp(){
  // SI7021 sends 2-byte data, MSB then LSB (big-endian)
  // uint16_t is little-endian, so you can't directly store to uint16_t
  // add values from 0
  uint16_t sensor_value = read_data[1];
  sensor_value += (uint16_t)read_data[0] << 8;

  // convert sensor value to temperature
  float sensor_temp = SI7021_convert_temp(sensor_value);
  int rounded_sensor_temp = (int)sensor_temp;

  return rounded_sensor_temp;
}

uint8_t* getReadData_buf(){
  return &(read_data[0]);
}

// Using I2CSPM for initialization
void VEML6030_initialize(){
  // Operation Mode - VDD = 3.3 V, PSM = 11, refresh time 4100 ms, ALS_GAIN = “01” for minimum gain
  // For VEML6030, 3 byte data is sent in cmd-data_LSB-data_MSB
  I2C_TransferReturn_TypeDef transferStatus;

  // Set mode
  VEML6030_cmd_data[0] = VEML6030_ALS_CONF_0;
  VEML6030_cmd_data[1] = 0b00000000; // ALS_PERS = 00, Protect # = 1, ALS_INT_EN = 0, ALS_SD = 0 (ALS power on)
  VEML6030_cmd_data[2] = 0b00001011; // ALS_GAIN = 01, ALS_IT = 1100 for 25ms integration time

  transferSequence.addr = VEML6030_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE;
  transferSequence.buf[0].data = &(VEML6030_cmd_data[0]); // pointer to data to write
  transferSequence.buf[0].len = 3;

  transferStatus = I2CSPM_Transfer (I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
      LOG_ERROR("TransferStatus: %d\r\n", transferStatus); // currently NAK
      LOG_ERROR("I2C transfer Failed\r\n");
  }
  else{
      LOG_INFO("ALC_CONF complete\r\n");
  }

  transferStatus = I2CSPM_Transfer (I2C0, &transferSequence);
    if (transferStatus != i2cTransferDone) {
        LOG_ERROR("TransferStatus: %d\r\n", transferStatus); // currently NAK
        LOG_ERROR("I2C transfer Failed\r\n");
    }
    else{
        LOG_INFO("ALC_CONF complete\r\n");
    }

  // Power Saving Mode
  VEML6030_cmd_data[0] = VEML6030_POWER_SAVING;
  VEML6030_cmd_data[1] = 0b111; // PSM = 11, PSM_EN = 1
  VEML6030_cmd_data[2] = 0b0;

  transferSequence.addr = VEML6030_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE;
  transferSequence.buf[0].data = &(VEML6030_cmd_data[0]); // pointer to data to write
  transferSequence.buf[0].len = 3;

  transferStatus = I2CSPM_Transfer (I2C0, &transferSequence);
  if (transferStatus != i2cTransferDone) {
     LOG_ERROR("TransferStatus: %d\r\n", transferStatus); // currently NAK
     LOG_ERROR("I2C transfer Failed\r\n");
  }
  else{
      LOG_INFO("Power Saving Mode complete\r\n");
  }

}

// since refresh time = 4100ms, call this every >5 sec
void VEML6030_start_read_ambient_light_level(){
  set_I2C_transfer_target(I2C_TRANSFER_VEML6030);

  // Set mode
  VEML6030_cmd_data[0] = VEML6030_ALS;

  transferSequence.addr = VEML6030_ADDR << 1; // shift device address left
  transferSequence.flags = I2C_FLAG_WRITE_READ;
  transferSequence.buf[0].data = &(VEML6030_cmd_data[0]); // write cmd = register
  transferSequence.buf[0].len = 1;
  transferSequence.buf[1].data = &(VEML6030_read_data[0]);
  transferSequence.buf[1].len = 2;


  I2C_TransferReturn_TypeDef transferStatus;
  // transferStatus = I2C_TransferInit(I2C0, &transferSequence);
  transferStatus = I2CSPM_Transfer(I2C0, &transferSequence);
  if (transferStatus < 0) {
    LOG_ERROR("%d\r\n", transferStatus);
  }
  else{
    LOG_INFO("data[0:1] - %d %d\r\n", VEML6030_read_data[0],VEML6030_read_data[1]);
  }
}

uint16_t VEML6030_read_measured_ambient_light(){

  // sends 2-byte data, MSB then LSB (big-endian)
  // uint16_t is little-endian, so you can't directly store to uint16_t
  // add values from 0
  uint16_t sensor_value = VEML6030_read_data[0];
  sensor_value += (uint16_t)VEML6030_read_data[1] << 8;

  return sensor_value;
}


int8_t getSoundLevel(){
  return rand() % 20;
}

#endif
