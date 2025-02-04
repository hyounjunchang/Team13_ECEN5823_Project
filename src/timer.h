/***********************************************************************
 * @file      timer.h
 * @brief     Header for timer functions
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 30, 2025
 *
 * @resources
 *            List of CMU functions can be found at https://siliconlabs.github.io/Gecko_SDK_Doc/efm32g/html/group__CMU.html
 *
 * Editor: Jan 30, 2025 Hyounjun Chang
 * Change: Initial .h file definition
 *
 */
#ifndef GECKO_TIMERS
#define GECKO_TIMERS

#include <stdint.h> // for uint32_t declaration

void init_LETIMER0();

// waits for at least us_wait microseconds
void timerWaitUs(uint32_t us_wait);


#endif // GECKO_TIMERS
