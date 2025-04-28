/***********************************************************************
 * @file      scheduler.c
 * @brief     Task Scheduler for Blue Gecko, Keeps state of Si7021
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Mar 20, 2025
 *
 * @resources ECEN5823 Lecture 6 example code
 *
 *
 */
#include "scheduler.h"

#include <em_core.h>
#include <stdint.h>

// to control Si7021/GPIO
#include "gpio.h"
#include "i2c.h"

// for ble status
#include "ble.h"

#include "autogen/gatt_db.h"

#include "lcd.h"

#include "src/em_adc.h"
#include "src/adc.h"

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#if DEVICE_IS_BLE_SERVER
static uint16_t VEML6030_timer_count = 0;
#endif

// edited from Lecture 6 slides
// sets event flag for future use
void set_scheduler_event(scheduler_event event){
#if DEVICE_IS_BLE_SERVER
  sl_status_t sc;
  unsigned int PB0_val;
  CORE_DECLARE_IRQ_STATE;
  switch (event){
    case NO_EVENT:
      break;
    // prioritize I2C transfer event first
    case EVENT_I2C_TRANSFER:
     CORE_ENTER_CRITICAL();
     sc = sl_bt_external_signal(BLE_I2C_VEML6030_TRANSFER_FLAG);
     CORE_EXIT_CRITICAL();
     if (sc != SL_STATUS_OK){
         LOG_ERROR("Error setting BLE_I2C_VEML6030_TRANSFER_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
     }
     break;
    case EVENT_LETIMER0_COMP1:
      CORE_ENTER_CRITICAL();
      sc = sl_bt_external_signal(BLE_LETIMER0_COMP1_FLAG);
      CORE_EXIT_CRITICAL();
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting BLE_LETIMER0_COMP1_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    case EVENT_LETIMER0_UF:
      CORE_ENTER_CRITICAL();
      sc = sl_bt_external_signal(BLE_LETIMER0_UF_FLAG);
      CORE_EXIT_CRITICAL();
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting BLE_LETIMER0_UF_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    case EVENT_PB:
      PB0_val = gpioRead_PB0(); // 1 if released, 0 if pressed
      if (PB0_val){
          CORE_ENTER_CRITICAL();
          sc = sl_bt_external_signal(BLE_PB0_RELEASE);
          CORE_EXIT_CRITICAL();
          if (sc != SL_STATUS_OK){
              LOG_ERROR("Error setting BLE_PB0_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
          }
      }
      else{
          CORE_ENTER_CRITICAL();
          sc = sl_bt_external_signal(BLE_PB0_PRESS);
          CORE_EXIT_CRITICAL();
          if (sc != SL_STATUS_OK){
              LOG_ERROR("Error setting BLE_PB0_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
          }
      }
      break;
    default:
      break;
  }
#endif
}

#if DEVICE_IS_BLE_SERVER

// Read Ambient Light every 5 sec
void ambient_light_state_machine(sl_bt_msg_t* evt){
  // only update on external signal (non-bluetooth)
  if (SL_BT_MSG_ID(evt->header) != sl_bt_evt_system_external_signal_id){
      return;
  }
  uint32_t ble_event_flags = evt->data.evt_system_external_signal.extsignals;

  // start reading VEML6030 every 5 second
  if (ble_event_flags & BLE_LETIMER0_UF_FLAG){
      if (VEML6030_timer_count == 0){
          VEML6030_start_read_ambient_light_level();
          VEML6030_timer_count++;
      }
      else if(VEML6030_timer_count == 4){
          float amb_light_lux = VEML6030_read_measured_ambient_light();
          update_amb_light_gatt_and_send_notification(amb_light_lux);
          VEML6030_timer_count = 0;
      }
      else {
          VEML6030_timer_count++;
      }
  }
}

void lcd_display_update(sl_bt_msg_t* evt){
  // only update on external signal (non-bluetooth)
  if (SL_BT_MSG_ID(evt->header) != sl_bt_evt_system_external_signal_id){
      return;
  }
  uint32_t ble_event_flags = evt->data.evt_system_external_signal.extsignals;

  // refresh LCD every 1s
  if (ble_event_flags & BLE_LETIMER0_UF_FLAG){
      displayUpdate(); // prevent charge buildup within the Liquid Crystal Cells
  }
}
#endif

