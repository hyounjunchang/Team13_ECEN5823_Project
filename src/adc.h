/***********************************************************************
 * @file      adc.h
 * @brief     ADC module header for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Apr 21, 2025
 *
 * @resources ECEN5823 final project
 *
 *
 *
 */
#ifndef SRC_ADC_H_
#define SRC_ADC_H_

#include <stdint.h>
#include <stdbool.h>

void initADC();
void getScannedADCmV(uint32_t* adc_buf);
bool OKtoUpdateGATT();
void setOKtoUpdateGATT(bool ok);
#endif
