/***********************************************************************
 * @file      irq.c
 * @brief     Functions for Interrupt Service Handlers (IRQ)
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 26, 2025
 *
 * @resources
 *
 *
 * Editor: Jan 26, 2025 Hyounjun Chang
 * Change: Initial IRQ handlers definition
 *
 */

#include "irq.h"

// for GPIO control
#include "em_gpio.h"
#include "gpio.h"

// for timer/register control
#include "em_letimer.h"
#include <em_core.h>

#include <stdbool.h>

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#define LED_port   gpioPortF
#define LED0_pin   4 // PF4

// check startup_efr32bg13p.c for list of IRQ Handlers
// default is "weak" definition, for details, read https://stackoverflow.com/questions/51656838/attribute-weak-and-static-libraries
void LETIMER0_IRQHandler(){
  // Log for debugging
  LOG_INFO("IRQ Triggered\n");
  CORE_DECLARE_IRQ_STATE;


  // Timer IRQ explanation:
  // COMP0 is set to num_clocks for period, COMP1 is set to num_clocks for LED on
  // LED is on when COMP1 flag is set, turned off when underflow flag is set

  // step 1: determine pending interrupts in peripheral
  uint32_t interrupt_flag = LETIMER_IntGet(LETIMER0); // from LETIMER0->IF;

  bool turn_LED_on = interrupt_flag & LETIMER_IEN_COMP1;
  bool turn_LED_off = interrupt_flag & LETIMER_IEN_UF;

  // step 2: clear pending interrupts in peripheral
  LETIMER_IntClear(LETIMER0, 0xFFFFFFFF); // LETIMER0->IFC clear underflow flag

  // step 3: your handling code
  bool LED_on = GPIO_PinInGet(LED_port, LED0_pin);
  if (turn_LED_on){
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      gpioLed0SetOn();
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
  }
  else if (turn_LED_off){
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      gpioLed0SetOff();
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
  }

  // enable Interrupt again
  uint32_t letimer0_flags = LETIMER_IEN_UF|LETIMER_IEN_COMP1; // set interrupt flags again
  LETIMER_IntEnable(LETIMER0, letimer0_flags);

}
