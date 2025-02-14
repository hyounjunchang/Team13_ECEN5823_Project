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

#define LETIMER0_TOP_VALUE (LETIMER_PERIOD_MS * LETIMER0_FREQ) / 1000

#define NUM_MIROSEC_IN_SEC 1000000
//
#define LETIMER_CNT_TO_MS(x) (x * 1000) / LETIMER0_FREQ


// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

static bool timerwait_done = false;

// last requested letimer interrupt duration in ms
static uint32_t letimer_uf_duration_ms = 0;

void init_LETIMER0(){
  // Update last_letimer_duration_ms
  letimer_uf_duration_ms = LETIMER_PERIOD_MS;

  // disable LETIMER0 in case its already initialized
  LETIMER_Enable(LETIMER0, false);

  // example code from lecture 5
  uint32_t temp;

  // Clear all IRQ flags in the LETIMER0 IF status register
  LETIMER_IntClear (LETIMER0, 0xFFFFFFFF); // punch them all down
  // Disable all interrupts before re-enabling... (since we might have used different interrupts)
  LETIMER_IntDisable(LETIMER0, 0xFFFFFFFF);

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
  LETIMER_CompareSet(LETIMER0, 0, LETIMER0_TOP_VALUE); //value has to be within 16-bits

  // Set UF flag in LETIMER0_IEN, so that the timer will generate IRQs to the NVIC.
  temp = LETIMER_IEN_UF;
  LETIMER_IntEnable (LETIMER0, temp); // Make sure you have defined the ISR routine LETIMER0_IRQHandler()

  // Enable the timer to starting counting down, set LETIMER0_CMD[START] bit, see LETIMER0_STATUS[RUNNING] bit
  LETIMER_Enable (LETIMER0, true);
}

/*
// waits for at least us_wait microseconds, blocking
// uses COMP1 register
void timerWaitUs_polled(uint32_t us_wait){
  uint64_t num_timer_cycles_remaining = ((uint64_t)us_wait * (uint64_t)LETIMER0_FREQ /
                               (uint64_t)NUM_MIROSEC_IN_SEC) + 1; // 64-bit due to truncation

  // disable going to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM2);
  }

  while (num_timer_cycles_remaining != 0){
      clear_timerwait_done();
      // stop timer
      LETIMER_Enable(LETIMER0, false);

      // calculate new COMP1 value
      uint32_t curr_cnt = LETIMER_CounterGet(LETIMER0);
      uint32_t num_cycles_curr_irq;
      if (num_timer_cycles_remaining <= 0xFFFF){
          num_cycles_curr_irq = num_timer_cycles_remaining;
      }
      else{
          num_cycles_curr_irq = 0xFFFF;
      }

      uint32_t comp1_cnt;
      if (curr_cnt < num_cycles_curr_irq){
          comp1_cnt = curr_cnt + LETIMER0_TOP_VALUE - num_cycles_curr_irq;
      }
      else{
          comp1_cnt = curr_cnt - num_cycles_curr_irq;
      }

      // update COMP1, enable interrupt and start
      LETIMER_CompareSet(LETIMER0, 1, comp1_cnt);
      LETIMER_IntEnable (LETIMER0, LETIMER_IEN_COMP1);
      LETIMER_Enable(LETIMER0, true);

      // wait until ISR triggered
      while (!timerwait_done){
         //LOG_INFO("looping until ISR\n");
      }
      num_timer_cycles_remaining -= num_cycles_curr_irq;
  }

  // go to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM2);
  }
}
*/


// waits for at least us_wait microseconds, non-blocking
// uses LETIMER0_COMP1 register
// causes problems if used alongside timerWaitUS_polled()
// has limit of 16-bit timer value
void timerWaitUs_irq(uint32_t us_wait){
  uint64_t num_timer_cycles = ((uint64_t)us_wait * (uint64_t)LETIMER0_FREQ /
                               (uint64_t)NUM_MIROSEC_IN_SEC) + 1; // 64-bit due to truncation

  if (num_timer_cycles > 0xFFFF){
      LOG_ERROR("timerWaitUs_irq() out of range, NO TIMER WILL BE SET!\r\n");
      return;
  }

  LETIMER_Enable(LETIMER0, false); // briefly disable until comp1 set
  // Calculate COMP1 value
  uint32_t curr_cnt = LETIMER_CounterGet(LETIMER0);
  uint32_t comp1_cnt;
  if (curr_cnt < num_timer_cycles){
      comp1_cnt = curr_cnt + LETIMER0_TOP_VALUE - num_timer_cycles;
  }
  else{
      comp1_cnt =  curr_cnt - num_timer_cycles;
  }
  LETIMER_CompareSet(LETIMER0, 1, comp1_cnt);

  // LETIMER_IntEnable() must hanve LETIMER_IntClear()
  LETIMER_IntClear(LETIMER0, LETIMER_IEN_COMP1);
  LETIMER_IntEnable(LETIMER0, LETIMER_IEN_COMP1);
  LETIMER_Enable(LETIMER0, true);
}

bool get_timerwait_done(){
  return timerwait_done;
}

void set_timerwait_done(){
  timerwait_done = true;
}

void clear_timerwait_done(){
  timerwait_done = false;
}

uint32_t get_LETIMER_UF_duration_ms(){
  return letimer_uf_duration_ms;
}

uint32_t get_LETIMER_TOP_value(){
  return LETIMER0_TOP_VALUE;
}

uint32_t get_LETIMER_freq(){
  return LETIMER0_FREQ;
}
