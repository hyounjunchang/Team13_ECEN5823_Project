/***********************************************************************
 * @file      irq.h
 * @brief     Header for interrupt handlers (IRQ)
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 30, 2025
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
void enable_SI7021_event();
void disable_SI7021_event();

void add_letimerMilliseconds(uint32_t ms);
uint32_t letimerMilliseconds();

#endif
