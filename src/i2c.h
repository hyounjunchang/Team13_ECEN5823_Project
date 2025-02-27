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

void initialize_I2C();

// SI7021 functions
float SI7021_convert_temp(uint16_t temp_code);
void SI7021_start_measure_temp();
void SI7021_wait_temp_sensor();
void SI7021_start_read_sensor();
int SI7021_read_measured_temp();

uint8_t* getReadData_buf();

#endif /* SRC_I2C_H_ */
