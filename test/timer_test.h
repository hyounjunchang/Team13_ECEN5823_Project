/***********************************************************************
 * @file      timer_test.h
 * @brief     Header for timer.c unit test
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 13, 2025
 *
 * @resources
 *
 *
 */

#ifndef TEST_TIMER_TEST_H_
#define TEST_TIMER_TEST_H_

#include <stdint.h> // for uint32_t declaration

//void TEST_MODE_timerWaitUs_polled_LED_blink(uint32_t us);

void TEST_MODE_timerWaitUs_irq_LED_toggle(uint32_t us);
void TEST_MODE_reset_timerwait_irq_state();

void delayApprox(volatile int delay);

#endif /* TEST_TIMER_TEST_H_ */
