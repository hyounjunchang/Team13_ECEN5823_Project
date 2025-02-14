/***********************************************************************
 * @file      timer_test.c
 * @brief     timer.c unit test
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 3, 2025
 *
 * @resources
 *
 *
 * Editor: Feb 3, 2025 Hyounjun Chang
 * Change: Initial timer unit test
 *
 */

#include "src/timer.h"
#include "src/gpio.h"
#include "em_gpio.h"
#include "em_letimer.h"

/*
void TEST_MODE_timerWaitUs_polled_LED_blink(uint32_t us){
  gpioInit_LED();
  gpioLed0SetOn();
  timerWaitUs_polled(us);
  gpioLed0SetOff();
  timerWaitUs_polled(us);
}
*/

typedef enum {
  START_TIMER,
  WAIT_TIMER
} timer_irq_state;

static uint16_t timerwait_irq_state = START_TIMER;

void TEST_MODE_timerWaitUs_irq_LED_toggle(uint32_t us){
  // LETIMER_IntEnable() must hanve LETIMER_IntClear()
  LETIMER_IntClear(LETIMER0, LETIMER_IEN_COMP1);
  LETIMER_IntEnable(LETIMER0, LETIMER_IEN_COMP1);
  timer_irq_state NEXT_STATE = timerwait_irq_state;
  switch (timerwait_irq_state){
    case START_TIMER:
      timerWaitUs_irq(us);
      if(GPIO_PinInGet(gpioPortF, 4))
        delayApprox(350000);
      NEXT_STATE = WAIT_TIMER;
      break;
    case WAIT_TIMER: // do nothing if waiting timer
      break;
    default:
      break;
  }
  timerwait_irq_state = NEXT_STATE;
}

void TEST_MODE_reset_timerwait_irq_state(){
  timerwait_irq_state = START_TIMER;
}


// A value of 3500000 is ~ 1 second.
void delayApprox (int delay)
{
  volatile int i;

  for (i = 0; i < delay; ) {
      i=i+1;
  }

} // delayApprox()
