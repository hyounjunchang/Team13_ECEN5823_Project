/***********************************************************************
 * @file      irq.h
 * @brief     Header for interrupt handlers (IRQ)
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 13, 2025
 *
 * @resources
 *
 *
 *
 */

#ifndef IRQ_H
#define IRQ_H

#include <stdbool.h>
#include <stdint.h>

void LETIMER0_IRQHandler();

uint32_t letimerMilliseconds();
uint32_t get_LETIMER_UF_duration_ms();

#endif
