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
  gpioLed0SetOn();
  timerWaitUs(us);
  gpioLed0SetOff();
  timerWaitUs(us);
}
