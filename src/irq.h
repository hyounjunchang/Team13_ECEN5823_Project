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

void LETIMER0_IRQHandler();
void enable_events();
void disable_events();

#endif
