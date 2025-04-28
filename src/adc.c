/***********************************************************************
 * @file      adc.c
 * @brief     ADC module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Apr 21, 2025
 *
 * @resources Referenced from Silicon Labs Peripheral Examples
 * https://github.com/SiliconLabs/peripheral_examples/tree/master/series1/adc
 * em_adc.h and em_adc.c weren't available from gecko 4.3.2 sdk, so source header file was copied to /srcs
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
uint32_t adc_mv = 0; // ADC value in mV (max 2.5V)
bool ok_to_update_gatt = false;

/**************************************************************************//**
 * @brief ADC initialization
 * Source: https://github.com/SiliconLabs/peripheral_examples/blob/master/series1/adc/adc_single_letimer_interrupt/src/main_s1.c
 *****************************************************************************/
void initADC ()
{
  // Enable clocks required
  CMU_ClockEnable(cmuClock_HFPER, true);
  CMU_ClockEnable(cmuClock_ADC0, true);

  // Declare init structs
  ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
  ADC_InitSingle_TypeDef initSingle = ADC_INITSINGLE_DEFAULT;

  // Modify init structs
  init.prescale   = ADC_PrescaleCalc(adcFreq, 0);
  init.timebase   = ADC_TimebaseCalc(0);

  initSingle.diff       = false;       // single ended
  initSingle.reference  = adcRef2V5;   // internal 2.5V reference
  initSingle.resolution = adcRes12Bit; // 12-bit resolution
  initSingle.acqTime    = adcAcqTime4; // set acquisition time to meet minimum requirements

  // Select ADC input. See README for corresponding EXP header pin.
  initSingle.posSel = adcPosSelAPORT2XCH19; //Using PF3, APORT2XCH19

  // Initialize ADC and Single conversions
  ADC_Init(ADC0, &init);
  ADC_InitSingle(ADC0, &initSingle);

  // Enable ADC Single Conversion Complete interrupt
  ADC_IntEnable(ADC0, ADC_IEN_SINGLE);

  // Enable ADC interrupts
  NVIC_ClearPendingIRQ(ADC0_IRQn);
  NVIC_EnableIRQ(ADC0_IRQn);

}
