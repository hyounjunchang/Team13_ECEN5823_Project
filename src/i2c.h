/***********************************************************************
 * @file      i2c.h
 * @brief     I2C module header for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 4, 2025
 *
 * @resources
 *
 *
 * Editor: Feb 4, 2025 Hyounjun Chang
 * Change: Initial .h file definition
 *
 */


#ifndef SRC_I2C_H_
#define SRC_I2C_H_

#include <stdint.h>

void initialize_I2C();

// SI7021 functions
int SI7021_get_temperature();
float SI7021_convert_temp(uint16_t temp_code);

void SI7021_start_measure_temp();
uint16_t SI7021_read_measured_temp();

#endif /* SRC_I2C_H_ */
