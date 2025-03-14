/***********************************************************************
 * @file      irq.h
 * @brief     Header for interrupt handlers (IRQ)
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Mar 14, 2025
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
void I2C0_IRQHandler();

uint32_t letimerMilliseconds();
#endif
