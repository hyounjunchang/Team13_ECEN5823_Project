/***********************************************************************
 * @file      timer.h
 * @brief     Header for timer functions
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 26, 2025
 *
 * @resources
 *            List of CMU functions can be found at https://siliconlabs.github.io/Gecko_SDK_Doc/efm32g/html/group__CMU.html
 *
 * Editor: Jan 26, 2025 Hyounjun Chang
 * Change: Initial .h file definition
 *
 */

#ifndef GECKO_TIMERS
#define GECKO_TIMERS

#define LETIMER_ON_TIME_MS 175
#define LETIMER_PERIOD_MS 2250

#define LFXO_FREQ 32768
#define ULFRCO_FREQ 1000


#if (LOWEST_ENERGY_MODE == 3)
  #define LETIMER0_PRESCALER 1 // CMU_LFAPRESC0 register, set by CMU_ClockDivSet()
  #define LETIMER0_FREQ ULFRCO_FREQ/LETIMER0_PRESCALER
#else
  #define LETIMER0_PRESCALER 4 // CMU_LFAPRESC0 register, set by CMU_ClockDivSet()
  #define LETIMER0_FREQ LFXO_FREQ/LETIMER0_PRESCALER
#endif

#define NUM_TIMER_PERIOD (LETIMER_PERIOD_MS * LETIMER0_FREQ) / 1000
#define NUM_TIMER_LED_ON (LETIMER_ON_TIME_MS * LETIMER0_FREQ) / 1000

void init_LETIMER0();

#endif // GECKO_TIMERS
