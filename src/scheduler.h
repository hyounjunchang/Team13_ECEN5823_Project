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
#include "ble_device_type.h"

// defined events
// there are not the bit-fields
typedef enum{
    NO_EVENT = 0,
    EVENT_LETIMER0_UF,
    EVENT_LETIMER0_COMP1,
    EVENT_I2C_TRANSFER
} scheduler_event;

#if DEVICE_IS_BLE_SERVER
// for server
typedef enum{
  SI7021_IDLE,
  SI7021_WAIT_POWER_UP,
  SI7021_WAIT_I2C_WRITE,
  SI7021_WAIT_I2C_READ_START,
  SI7021_WAIT_I2C_READ_COMPLETE
} SI7021_state;
#else
// for client
typedef enum{
  BLE_OFF,
  SCANNING,
  CHECK_GATT_SERVICE,
  CHECK_GATT_CHARACTERSTIC,
  SET_GATT_INDICATION,
  RECEIVE_TEMP_DATA
} ble_client_state;
#endif


// for use in app.c
// scheduler_event getNextEvent();

// for use by IRQ
void set_scheduler_event(scheduler_event event);

#if DEVICE_IS_BLE_SERVER // functions only for server
// for SI7021 state machine
SI7021_state get_SI7021_state();
// temp state machine using ble event
void temperature_state_machine(sl_bt_msg_t* evt);
#else // functions for client
// for client state machine
ble_client_state get_client_state();
// update state machine using ble event
void client_state_machine(sl_bt_msg_t* evt);
#endif

#endif
