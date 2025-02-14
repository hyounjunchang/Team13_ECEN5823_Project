/***********************************************************************
 * @file      scheduler.c
 * @brief     Task Scheduler for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 3, 2025
 *
 * @resources ECEN5823 Lecture 6 example code
 *
 *
 */
#include "scheduler.h"

#include <em_core.h>
#include <stdint.h>


// Define bit-flags
#define NO_FLAG 0x0
#define LETIMER0_UF_FLAG 0x1
#define LETIMER0_COMP1_FLAG 0x2
#define I2C_TRANSFER_FLAG 0x4

typedef struct {
  uint32_t flag_0;
} event_flags;

static event_flags active_events = {NO_FLAG};
static SI7021_state currState_SI7021 = SI7021_READ_TEMP_POWER_OFF;

// edited from Lecture 6 slides
// returns a scheduler_event among one of the events available
// clears returned event flag
scheduler_event getNextEvent(){
  CORE_DECLARE_IRQ_STATE;
  scheduler_event next_event = NO_EVENT;

  // clear flag and return event
  // This is different from register flags!!!
  if (active_events.flag_0){
      if (active_events.flag_0 & I2C_TRANSFER_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.flag_0 &= ~I2C_TRANSFER_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return EVENT_I2C_TRANSFER;
      }
      else if (active_events.flag_0 & LETIMER0_COMP1_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.flag_0 &= ~LETIMER0_COMP1_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return EVENT_LETIMER0_COMP1;
      }
      else if (active_events.flag_0 & LETIMER0_UF_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.flag_0 &= ~LETIMER0_UF_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return EVENT_LETIMER0_UF;
      }
  }

  return next_event;
}

// edited from Lecture 6 slides
// sets event flag for future use
void set_scheduler_event(scheduler_event event){
  CORE_DECLARE_IRQ_STATE;

  switch (event){
    case NO_EVENT:
      break;
    case EVENT_LETIMER0_UF:
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      active_events.flag_0 |= LETIMER0_UF_FLAG;
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
      break;
    case EVENT_LETIMER0_COMP1:
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      active_events.flag_0 |= LETIMER0_COMP1_FLAG;
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
      break;
    case EVENT_I2C_TRANSFER:
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      active_events.flag_0 |= I2C_TRANSFER_FLAG;
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
      break;
    default:
      break;
  }
}

void update_SI7021_state_machine(scheduler_event event){
  SI7021_state nextState_SI7021 = currState_SI7021;
  switch(currState_SI7021){
    case SI7021_READ_TEMP_POWER_OFF:
      if (event == EVENT_LETIMER0_UF){
          nextState_SI7021 = SI7021_POWER_ON_RESET;
      }
      break;
    case SI7021_POWER_ON_RESET:
      if (event == EVENT_LETIMER0_COMP1){
          nextState_SI7021 = SI7021_I2C_INITIATE_SENSOR;
      }
      break;
    case SI7021_I2C_INITIATE_SENSOR:
      if (event == EVENT_I2C_TRANSFER){
          nextState_SI7021 = SI7021_WAIT_SENSOR;
      }
      break;
    case SI7021_WAIT_SENSOR:
      if (event == EVENT_LETIMER0_COMP1){
          nextState_SI7021 = SI7021_I2C_READ_SENSOR;
      }
      break;
    case SI7021_I2C_READ_SENSOR:
      if (event == EVENT_I2C_TRANSFER){
          nextState_SI7021 = SI7021_READ_TEMP_POWER_OFF;
      }
      break;
    default:
      break;
  }
  currState_SI7021 = nextState_SI7021;
}

SI7021_state get_SI7021_state(){
  return currState_SI7021;
}
