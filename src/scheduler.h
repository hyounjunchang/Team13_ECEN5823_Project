/***********************************************************************
 * @file      scheduler.h
 * @brief     Header Program Scheduler
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Mar 14, 2025
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
    EVENT_I2C_TRANSFER,
    EVENT_PB
} scheduler_event;

#if DEVICE_IS_BLE_SERVER

typedef enum{
  VEML6030_IDLE,
  VEML6030_WAIT_I2C_READ
} VEML6030_state;


#endif


// for use in app.c
// scheduler_event getNextEvent();

// for use by IRQ
void set_scheduler_event(scheduler_event event);

#if DEVICE_IS_BLE_SERVER // functions only for server

// state machines using ble event
void temperature_state_machine(sl_bt_msg_t* evt);
void ambient_light_state_machine(sl_bt_msg_t* evt);

void sound_detector_update(sl_bt_msg_t* evt);
void lcd_display_update(sl_bt_msg_t* evt);

#endif

#endif
