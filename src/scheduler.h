/***********************************************************************
 * @file      scheduler.h
 * @brief     Header Program Scheduler
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 3, 2025
 *
 * @resources
 *
 *
 * Editor: Feb 3, 2025 Hyounjun Chang
 * Change: Initial .h file definition
 *
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

// defined events
// there are not the bit-fields
typedef enum{
    NO_EVENT = 0,
    LETIMER0_UF
} scheduler_event;


// for use in app.c
scheduler_event getNextEvent();

// for use by IRQ
void set_scheduler_event(scheduler_event event);


#endif
