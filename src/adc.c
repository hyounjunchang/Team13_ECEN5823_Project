/***********************************************************************
 * @file      adc.c
 * @brief     ADC module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Apr 21, 2025
 *
 * @resources Referenced from Silicon Labs Peripheral Examples
 * https://github.com/SiliconLabs/peripheral_examples/blob/master/series1/adc/adc_scan_letimer_interrupt/src/main_tg11.c
 * em_adc.h wasn't available from gecko 4.3.2 sdk, so source header file was copied to /srcs
 *
 *
 */
#include "adc.h"
#include <stdint.h>

#include <stdio.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_letimer.h"

// Note: em_adc.h was not included in Gecko 4.3.2 sdk, so source header file was copied over to /src
// from https://github.com/SiliconLabs/gecko_sdk/blob/59d2160fe448b13730a91c0f019a8b4c53c43eb2/platform/emlib/inc/em_adc.h
#include "src/em_adc.h"

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


// Init to max ADC clock for Series 1
#define adcFreq   16000000

#define NUM_INPUTS 1
uint32_t adc_value = 0; // ADC value in mV (max 2.5V)

/**************************************************************************//**
 * @brief ADC initialization
 *****************************************************************************/
void initADC ()
{
  // Enable clocks required
  CMU_ClockEnable(cmuClock_HFPER, true);
  CMU_ClockEnable(cmuClock_ADC0, true);

  // Declare init structs
  ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
  ADC_InitScan_TypeDef initScan = ADC_INITSCAN_DEFAULT;

  // Modify init structs
  init.prescale = ADC_PrescaleCalc(adcFreq, 0);
  init.timebase = ADC_TimebaseCalc(0);

  initScan.diff          = false;       // single ended
  initScan.reference     = adcRef2V5;   // internal 2.5V reference, Sensor outputs 2.7V
  initScan.resolution    = adcRes12Bit; // 12-bit resolution
  initScan.acqTime       = adcAcqTime4; // set acquisition time to meet minimum requirements
  initScan.fifoOverwrite = true;        // FIFO overflow overwrites old data

  // Select ADC input. See README for corresponding EXP header pin.
  // *Note that internal channels are unavailable in ADC scan mode
  ADC_ScanSingleEndedInputAdd(&initScan, adcScanInputGroup0, adcPosSelAPORT2XCH19); // Using PF3, APORT2XCH19

  // Set scan data valid level (DVL) to 2
  ADC0->SCANCTRLX = (ADC0->SCANCTRLX & ~_ADC_SCANCTRLX_DVL_MASK) | (((NUM_INPUTS - 1) << _ADC_SCANCTRLX_DVL_SHIFT) & _ADC_SCANCTRLX_DVL_MASK);

  // Clear ADC scan FIFO
  ADC0->SCANFIFOCLEAR = ADC_SCANFIFOCLEAR_SCANFIFOCLEAR;

  // Initialize ADC and Scans
  ADC_Init(ADC0, &init);
  ADC_InitScan(ADC0, &initScan);

  // Enable Scan interrupts
  ADC_IntEnable(ADC0, ADC_IEN_SCAN);

  // Enable ADC Interrupts
  NVIC_ClearPendingIRQ(ADC0_IRQn);
  NVIC_EnableIRQ(ADC0_IRQn);
}

void startADCscan(){
  // Start next ADC conversion
  ADC_Start(ADC0, adcStartSingle);
}

uint32_t getScannedADCdata(){
  uint32_t data;

  // Get data from ADC scan
  data = ADC_DataScanGet(ADC0);

  // Convert data to mV and store to array
  adc_value = data * 2500 / 4096;

  return adc_value;
}

uint32_t getLastADCdata(){
  return adc_value;
}


