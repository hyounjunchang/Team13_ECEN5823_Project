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

// for configuring I2C
void initialize_I2C();

// Ambient Light Sensor
void VEML6030_initialize();
void VEML6030_start_read_ambient_light_level();
float VEML6030_convert_to_lux(uint16_t adc_val);
float VEML6030_read_measured_ambient_light();

#endif

#endif /* SRC_I2C_H_ */
