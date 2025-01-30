/***********************************************************************
 * @file      timer.c
 * @brief     Timer functions (non-ISR) for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 26, 2025
 *
 * @resources Silicon Labs peripheral example
 *            https://github.com/SiliconLabs/peripheral_examples/blob/master/series1/adc/adc_single_letimer_prs_dma/src/main_s1.c
 *
 *
 * Editor: Jan 26, 2025 Hyounjun Chang
 * Change: Initial timer function setup
 *
 */
#include "app.h"
#include "timer.h"
#include "em_letimer.h"

#define LETIMER_ON_TIME_MS 175
#define LETIMER_PERIOD_MS 2250

#define LFXO_FREQ 32768
#define ULFRCO_FREQ 1000

#if (LOWEST_ENERGY_MODE == 3)
  #define LETIMER0_PRESCALER 1 // CMU_LFAPRESC0 register, set by CMU_ClockDivSet()
  #define LETIMER0_FREQ ULFRCO_FREQ/LETIMER0_PRESCALER
#else
  #define LETIMER0_PRESCALER 4 // CMU_LFAPRESC0 register, set by CMU_ClockDivSet()
  #define LETIMER0_FREQ LFXO_FREQ/LETIMER0_PRESCALER
#endif

#define NUM_TIMER_PERIOD (LETIMER_PERIOD_MS * LETIMER0_FREQ) / 1000
#define NUM_TIMER_LED_ON (LETIMER_ON_TIME_MS * LETIMER0_FREQ) / 1000

void init_LETIMER0(){
  // example code from lecture 5
  uint32_t temp;

  // this data structure is passed to LETIMER_Init (), used to set LETIMER0_CTRL reg bits and other values
  const LETIMER_Init_TypeDef letimerInitData = {
  false, // enable; don't enable when init completes, we'll enable last
    true, // debugRun; useful to have the timer running when single-stepping in the debugger
    true, // comp0Top; load COMP0 into CNT on underflow
    false, // bufTop; don't load COMP1 into COMP0 when REP0==0
    0, // out0Pol; 0 default output pin value
    0, // out1Pol; 0 default output pin value
    letimerUFOANone, // ufoa0; no underflow output action
    letimerUFOANone, // ufoa1; no underflow output action
    letimerRepeatFree, // repMode; free running mode i.e. load & go forever
    0 // COMP0(top) Value, I calculate this below
  };

  // init the timer
  LETIMER_Init (LETIMER0, &letimerInitData);

  // Timer Value explanation:
  // COMP0 is set to num_clocks for period, COMP1 is set to num_clocks for LED on
  // LED is on when COMP1 flag is set, turned off when underflow flag is set

  // calculate and load COMP0 (top)
  LETIMER_CompareSet(LETIMER0, 0, NUM_TIMER_PERIOD); //value has to be within 16-bits
  // calculate and load COMP1
  LETIMER_CompareSet(LETIMER0, 1, NUM_TIMER_LED_ON); // num cycles for LED_on

  // Clear all IRQ flags in the LETIMER0 IF status register
  LETIMER_IntClear (LETIMER0, 0xFFFFFFFF); // punch them all down

  // Set UF and COMP1 in LETIMER0_IEN, so that the timer will generate IRQs to the NVIC.
  temp = LETIMER_IEN_UF | LETIMER_IEN_COMP1;
  LETIMER_IntEnable (LETIMER0, temp); // Make sure you have defined the ISR routine LETIMER0_IRQHandler()

  // Enable the timer to starting counting down, set LETIMER0_CMD[START] bit, see LETIMER0_STATUS[RUNNING] bit
  LETIMER_Enable (LETIMER0, true);
}

