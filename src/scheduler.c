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
#define LETIMER0_UF_FLAG LETIMER_IEN_UF



typedef struct {
  uint32_t letimer0_flags;
} event_flags;

static event_flags active_events = {NO_FLAG};

// edited from Lecture 6 slides
// returns a scheduler_event among one of the events available
// clears returned event flag
scheduler_event getNextEvent(){
  CORE_DECLARE_IRQ_STATE;
  scheduler_event next_event = NO_EVENT;

  // clear flag and return event
  // This is different from register flags!!!
  if (active_events.letimer0_flags){
      if (active_events.letimer0_flags & LETIMER0_UF_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.letimer0_flags &= ~LETIMER0_UF_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return Si7021_LETIMER0_UF;
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
    case Si7021_LETIMER0_UF:
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      active_events.letimer0_flags |= LETIMER0_UF_FLAG;
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
      break;
    default:
      break;
  }


}
