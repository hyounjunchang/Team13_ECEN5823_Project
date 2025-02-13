/***********************************************************************
 * @file      irq.c
 * @brief     Functions for Interrupt Service Handlers (IRQ)
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 30, 2025
 *
 * @resources
 *
 *
 */

#include "irq.h"

// for GPIO control
#include "em_gpio.h"
#include "gpio.h"

// for timer/register control
#include "em_letimer.h"
#include <em_core.h>
#include "timer.h"

// for power mode
#include "app.h"

// add event to scheduler
#include "scheduler.h"

// add bool type
#include <stdbool.h>

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#ifdef TEST_MODE
  #include "test/timer_test.h"
#endif


// static variable, only for this scope
static uint32_t letimer_uf_count = 0;

// check startup_efr32bg13p.c for list of IRQ Handlers
// default is "weak" definition, for details, read https://stackoverflow.com/questions/51656838/attribute-weak-and-static-libraries
// referenced from ECEN5823 isr_and_scheduler_issues.txt
void LETIMER0_IRQHandler(){
  CORE_DECLARE_IRQ_STATE;

  // disable going to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM2);
  }

  // Timer IRQ explanation:
  // COMP0 is set to num_clocks for period, COMP1 is set to num_clocks for LED on
  // LED is on when COMP1 flag is set, turned off when underflow flag is set

  // step 1: determine pending interrupts in peripheral
  uint32_t interrupt_flags = LETIMER_IntGet(LETIMER0); // from LETIMER0->IF;

  // step 2: clear pending interrupts in peripheral
  LETIMER_IntClear(LETIMER0, interrupt_flags);

  // step 3: your handling code
  if (interrupt_flags & LETIMER_IEN_UF){
      set_scheduler_event(SI7021_LETIMER0_UF);
      // update ms_time on LETIMER_UF (underflow) interrupt
      CORE_ENTER_CRITICAL();
      letimer_uf_count++;
      CORE_EXIT_CRITICAL();
  }
  if (interrupt_flags & LETIMER_IEN_COMP1){
     // toggle led for test mode
     #ifdef TEST_MODE
       gpioLed0Toggle();
       TEST_MODE_reset_timerwait_irq_state();
     #endif

     set_timerwait_done(); // for timerWaitUs_polled()
     set_scheduler_event(EVENT_LETIMER0_COMP1);
     LETIMER_IntDisable(LETIMER0, LETIMER_IEN_COMP1); // needs to be disabled

  }
  // go to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM2);
  }

}

uint32_t letimerMilliseconds(){
  uint32_t time_elapsed = letimer_uf_count * get_LETIMER_UF_duration_ms();
  uint32_t curr_cnt = LETIMER_CounterGet(LETIMER0);
  // add time since last LETIMER0_UF
  time_elapsed += (get_LETIMER_TOP_value() - curr_cnt) * (uint32_t)1000 / get_LETIMER_freq();

  return time_elapsed;
}
