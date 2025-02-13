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
 *
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

// defined events
// there are not the bit-fields
typedef enum{
    NO_EVENT = 0,
    EVENT_LETIMER0_UF,
    EVENT_LETIMER0_COMP1,
    EVENT_I2C_TRANSFER
} scheduler_event;

typedef enum{
  SI7021_READ_TEMP_POWER_OFF,
  SI7021_POWER_ON_RESET,
  SI7021_I2C_INITIATE_SENSOR,
  SI7021_WAIT_SENSOR,
  SI7021_I2C_READ_SENSOR
} SI7021_state;


// for use in app.c
scheduler_event getNextEvent();

// for use by IRQ
void set_scheduler_event(scheduler_event event);

// for SI7021 state machine
void update_SI7021_state_machine(scheduler_event event);
SI7021_state get_SI7021_state();

#endif
