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

void initADC();
void startADCscan();
uint32_t getScannedADCdata();
uint32_t getLastADCdata();
#endif
