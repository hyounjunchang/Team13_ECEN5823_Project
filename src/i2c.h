/***********************************************************************
 * @file      i2c.h
 * @brief     I2C module header for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 13, 2025
 *
 * @resources
 *
 *
 *
 */
#ifndef SRC_I2C_H_
#define SRC_I2C_H_

#include <stdint.h>

#include "ble_device_type.h" // to determine type of device

#if DEVICE_IS_BLE_SERVER == 1 // BLE Server

// to determine what I2C is being used
typedef enum{
  I2C_TRANSFER_NONE,
  I2C_TRANSFER_SI7021,
  I2C_TRANSFER_VEML6030
} i2c_transfer_target;

// for configuring I2C
void initialize_I2C();
void set_I2C_transfer_target(i2c_transfer_target target);
void clear_I2C_transfer_target();
i2c_transfer_target get_I2C_transfer_target();

// SI7021 functions
float SI7021_convert_temp(uint16_t temp_code);
void SI7021_start_measure_temp();
void SI7021_wait_temp_sensor();
void SI7021_start_read_sensor();
int SI7021_read_measured_temp();

uint8_t* getReadData_buf();

// Ambient Light Sensor
void VEML6030_initialize();
void VEML6030_start_read_ambient_light_level();
float VEML6030_convert_to_lux(uint16_t adc_val);
float VEML6030_read_measured_ambient_light();

#endif

#endif /* SRC_I2C_H_ */
