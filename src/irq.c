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
 * Editor: Jan 30, 2025 Hyounjun Chang
 * Change: Added LETIMER0_IRQHandler to toggle LED0
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

#include <stdbool.h>

// add event to scheduler
#include "scheduler.h"

// add bool type
#include <stdbool.h>

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

// static variable, only for this scope
static bool events_enabled = true;

// check startup_efr32bg13p.c for list of IRQ Handlers
// default is "weak" definition, for details, read https://stackoverflow.com/questions/51656838/attribute-weak-and-static-libraries
// referenced from ECEN5823 isr_and_scheduler_issues.txt
void LETIMER0_IRQHandler(){

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

      // only add event if scheduler enabled
      if (events_enabled){
          set_scheduler_event(Si7021_LETIMER0_UF);
      }

      bool timerWait_flag = get_timerWait_flag();
      if (!timerWait_flag){
          set_timerWait_flag();
      }
  }

  // enable Interrupt again
  LETIMER_IntEnable(LETIMER0, interrupt_flags); // set interrupt flags

  // disable going to lower EM mode
  if (LOWEST_ENERGY_MODE == 1){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  else if (LOWEST_ENERGY_MODE == 2){
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM2);
  }

}

// enables scheduler event
void enable_events(){
  events_enabled = true;
}

// disables scheduler event
void disable_events(){
  events_enabled = false;
}
