/***********************************************************************
 * @file      irq.c
 * @brief     Functions for Interrupt Service Handlers (IRQ)
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Mar 20, 2025
 *
 * @resources ECEN5823 isr_and_scheduler_issues.txt, Lecture 8 Slides
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

// for I2C typedefs
#include <em_i2c.h>

#include "ble.h"

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

  // Timer IRQ explanation:
  // COMP0 is set to num_clocks for period, COMP1 is set to num_clocks for LED on
  // LED is on when COMP1 flag is set, turned off when underflow flag is set

  // step 1: determine pending interrupts in peripheral
  uint32_t interrupt_flags = LETIMER_IntGetEnabled(LETIMER0); // from LETIMER0->IF;

  // step 2: clear pending interrupts in peripheral
  LETIMER_IntClear(LETIMER0, interrupt_flags);

  // step 3: your handling code
  if (interrupt_flags & LETIMER_IEN_UF){
      set_scheduler_event(EVENT_LETIMER0_UF);
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
     LETIMER_IntDisable(LETIMER0, LETIMER_IEN_COMP1);
  }
}

uint32_t letimerMilliseconds(){
  uint32_t time_elapsed = letimer_uf_count * get_LETIMER_UF_duration_ms();
  uint32_t curr_cnt = LETIMER_CounterGet(LETIMER0);
  // add time since last LETIMER0_UF
  time_elapsed += (get_LETIMER_TOP_value() - curr_cnt) * (uint32_t)1000 / get_LETIMER_freq();

  return time_elapsed;
}

// From Lecture 8
void I2C0_IRQHandler(void) {
  /*
   * see em_i2.c
   * has access to global variables (transferSequence, cmd_data, read_data)
   * which were sent to I2C_TransferInit()
   */
  I2C_TransferReturn_TypeDef transferStatus;
  transferStatus = I2C_Transfer(I2C0);
  if (transferStatus == i2cTransferDone) {
      // set event
      set_scheduler_event(EVENT_I2C_TRANSFER);

      // Disable IRQ
      NVIC_DisableIRQ(I2C0_IRQn);

      //restore lowest EM mode, since we are done with I2C_Transfer
      if (LOWEST_ENERGY_MODE == 2){
         sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
         sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM2);
      }
      if (LOWEST_ENERGY_MODE == 3){
         sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
      }
  }
  if (transferStatus < 0) {
      LOG_ERROR("%d", transferStatus);
  }
} // I2C0_IRQHandler()

void GPIO_EVEN_IRQHandler(){
  // step 1: determine pending interrupts in peripheral
  uint32_t interrupt_flags = GPIO_IntGetEnabled();

  // step 2: clear pending interrupts in peripheral
  GPIO_IntClear(interrupt_flags);

  // step 3: your handling code
  // set event
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  set_scheduler_event(EVENT_PB);
  CORE_EXIT_CRITICAL();
}
