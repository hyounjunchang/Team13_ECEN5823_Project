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
 *
 */
#ifndef GECKO_TIMERS
#define GECKO_TIMERS

#include <stdint.h> // for uint32_t declaration
#include <stdbool.h>

void init_LETIMER0();

// waits for at least us_wait microseconds
// void timerWaitUs_polled(uint32_t us_wait); use timerWaitUs_irq instead
void timerWaitUs_irq(uint32_t us_wait);

// flags for timerWait
bool get_timerwait_done();
void set_timerwait_done();
void clear_timerwait_done();

// timer info
uint32_t get_LETIMER_UF_duration_ms();
uint32_t get_LETIMER_TOP_value();
uint32_t get_LETIMER_freq();

#endif // GECKO_TIMERS
