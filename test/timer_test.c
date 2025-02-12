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

void TimerWaitUs_LED_blink(uint32_t us){
  gpioInit_LED();
  gpioLed0SetOn();
  timerWaitUs_polled(us);
  gpioLed0SetOff();
  timerWaitUs_polled(us);
}
