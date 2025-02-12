/***********************************************************************
 * @file      timer.c
 * @brief     Timer functions (non-ISR) for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 30, 2025
 *
 * @resources Silicon Labs peripheral example
 *            https://github.com/SiliconLabs/peripheral_examples/blob/master/series1/adc/adc_single_letimer_prs_dma/src/main_s1.c
 *            ECEN5823 Lecture 5 example code
 *
 *
 */
#include "app.h"
#include "timer.h"
#include "em_letimer.h"

#include "irq.h" // to disable scheduler event

// for critical section
#include <em_core.h>

#define LETIMER_PERIOD_MS 3000

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


// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

static bool timerWait_flag = false;

void init_LETIMER0(){
  // disable LETIMER0 in case its already initialized
  LETIMER_Enable(LETIMER0, false);

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

  // calculate and load COMP0 (top)
  LETIMER_CompareSet(LETIMER0, 0, NUM_TIMER_PERIOD); //value has to be within 16-bits

  // Clear all IRQ flags in the LETIMER0 IF status register
  LETIMER_IntClear (LETIMER0, 0xFFFFFFFF); // punch them all down

  // Set UF flag in LETIMER0_IEN, so that the timer will generate IRQs to the NVIC.
  temp = LETIMER_IEN_UF;
  LETIMER_IntEnable (LETIMER0, temp); // Make sure you have defined the ISR routine LETIMER0_IRQHandler()

  // Enable the timer to starting counting down, set LETIMER0_CMD[START] bit, see LETIMER0_STATUS[RUNNING] bit
  LETIMER_Enable (LETIMER0, true);
}


// waits for at least us_wait microseconds
// if LETIMER0 frequency is low, wait will closer to ms range
// LETIMER0 must be initialized
void timerWaitUs(uint32_t us_wait){

  // disable going to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM2);
  }

  // disable IRQ from adding events
  disable_events();
  // 1sec = 1000000 us
  uint64_t num_timer_cycles = ((uint64_t)us_wait * (uint64_t)LETIMER0_FREQ / (uint64_t)1000000) + 1; // 64-bit due to truncation
  // update timer_wait
  timerWait_flag = false;

  while (num_timer_cycles != 0){
      LETIMER_Enable(LETIMER0, false);

      // Clear all IRQ flags in the LETIMER0 IF status register
      LETIMER_IntClear (LETIMER0, 0xFFFFFFFF);
      LETIMER_IntEnable (LETIMER0, LETIMER_IEN_UF);

      // set CNT variable
      if (num_timer_cycles <= 0xFFFF){
          LETIMER0->CNT = num_timer_cycles;
      }
      else{
          LETIMER0->CNT = 0xFFFF;
      }

      LETIMER_Enable(LETIMER0, true);

      // wait until ISR triggered
      while (!timerWait_flag){
         //LOG_INFO("looping until ISR\n");
      }

      if (num_timer_cycles > 0xFFFF){
          num_timer_cycles -= 0xFFFF;
      }
      else{
          num_timer_cycles = 0;
      }

      // reset timer wait flag
      timerWait_flag = false;
  }

  // enable IRQ to add events
  enable_events();

  // go to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM2);
  }

  init_LETIMER0();
}

bool get_timerWait_flag(){
  return timerWait_flag;
}

void set_timerWait_flag(){
  timerWait_flag = true;
}

void clear_timerWait_flag(){
  timerWait_flag = false;
}
