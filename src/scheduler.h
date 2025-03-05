/***********************************************************************
 * @file      scheduler.h
 * @brief     Header Program Scheduler
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 13, 2025
 *
 * @resources
 *
 *
 *
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "sl_bt_api.h"

// defined events
// there are not the bit-fields
typedef enum{
    NO_EVENT = 0,
    EVENT_LETIMER0_UF,
    EVENT_LETIMER0_COMP1,
    EVENT_I2C_TRANSFER
} scheduler_event;


#if DEVICE_IS_BLE_SERVER == 1
typedef enum{
  SI7021_IDLE,
  SI7021_WAIT_POWER_UP,
  SI7021_WAIT_I2C_WRITE,
  SI7021_WAIT_I2C_READ_START,
  SI7021_WAIT_I2C_READ_COMPLETE
} SI7021_state;
#endif


// for use in app.c
// scheduler_event getNextEvent();

// for use by IRQ
void set_scheduler_event(scheduler_event event);

#if DEVICE_IS_BLE_SERVER == 1 // functions only for server
// for SI7021 state machine
SI7021_state get_SI7021_state();
// temp state machine using ble event
void temperature_state_machine(sl_bt_msg_t* evt);
#else // functions for client

#endif

#endif
