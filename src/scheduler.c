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
static SI7021_state currState_SI7021 = SI7021_IDLE;

static VEML6030_state currState_VEML6030 = VEML6030_IDLE;
static uint16_t VEML6030_timer_count = 0;

i2c_transfer_target curr_i2c_transfer = I2C_TRANSFER_NONE;
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
     curr_i2c_transfer = get_I2C_transfer_target();
     if (curr_i2c_transfer == I2C_TRANSFER_SI7021){
         CORE_ENTER_CRITICAL();
         sc = sl_bt_external_signal(BLE_I2C_SI7021_TRANSFER_FLAG);
         CORE_EXIT_CRITICAL();

         if (sc != SL_STATUS_OK){
             LOG_ERROR("Error setting BLE_I2C_SI7021_TRANSFER_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
         }
     }
     else if (curr_i2c_transfer == I2C_TRANSFER_VEML6030){
         CORE_ENTER_CRITICAL();
         sc = sl_bt_external_signal(BLE_I2C_VEML6030_TRANSFER_FLAG);
         CORE_EXIT_CRITICAL();

         if (sc != SL_STATUS_OK){
             LOG_ERROR("Error setting BLE_I2C_VEML6030_TRANSFER_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
         }
     }
     // change I2C target
     clear_I2C_transfer_target();
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
SI7021_state get_SI7021_state(){
  return currState_SI7021;
}

// updates SI7021 state machine, and performs tasks to get to the next state)
void temperature_state_machine(sl_bt_msg_t* evt){
  // only update on external signal (non-bluetooth)
  if (SL_BT_MSG_ID(evt->header) != sl_bt_evt_system_external_signal_id){
      return;
  }

  uint32_t ble_event_flags = evt->data.evt_system_external_signal.extsignals;

  ble_data_struct_t* ble_data_ptr = get_ble_data();

  switch(currState_SI7021){
    case SI7021_IDLE:
      if ((ble_event_flags & BLE_LETIMER0_UF_FLAG) &&
          ble_data_ptr->ok_to_send_htm_notifications) {
          // take action and update state
          gpioPowerOn_SI7021();
          currState_SI7021 = SI7021_WAIT_POWER_UP;
      }
      break;
    case SI7021_WAIT_POWER_UP:
      if (ble_event_flags & BLE_LETIMER0_COMP1_FLAG){
          NVIC_EnableIRQ(I2C0_IRQn); // since updated state has I2C transfer, enable IRQ

          // take action and update state
          SI7021_start_measure_temp();
          currState_SI7021 = SI7021_WAIT_I2C_WRITE;
      }
      break;
    case SI7021_WAIT_I2C_WRITE:
      if (ble_event_flags & BLE_I2C_SI7021_TRANSFER_FLAG){
          // take action and update state
          SI7021_wait_temp_sensor();
          currState_SI7021 = SI7021_WAIT_I2C_READ_START;
      }
      break;
    case SI7021_WAIT_I2C_READ_START:
      if (ble_event_flags & BLE_LETIMER0_COMP1_FLAG){
          NVIC_EnableIRQ(I2C0_IRQn); // since updated state has I2C transfer, enable IRQ

          // take action and update state
          SI7021_start_read_sensor();
          currState_SI7021 = SI7021_WAIT_I2C_READ_COMPLETE;
      }
      break;
    case SI7021_WAIT_I2C_READ_COMPLETE:
      if (ble_event_flags & BLE_I2C_SI7021_TRANSFER_FLAG){
          // read temperature
          int curr_temperature = SI7021_read_measured_temp();
          //LOG_INFO("Read temperature %iC\r\n", curr_temperature);
          update_temp_meas_gatt_and_send_notification(curr_temperature);
          gpioPowerOff_SI7021();
          currState_SI7021 = SI7021_IDLE;
      }
      break;
    default:
      break;
  }
}

// Read Ambient Light every 5 sec
void ambient_light_state_machine(sl_bt_msg_t* evt){
  bool start_i2c = false;
  bool read_sensor = false;

  //uint32_t ble_event_flags = evt->data.evt_system_external_signal.extsignals;
  //ble_data_struct_t* ble_data_ptr = get_ble_data();
  uint16_t ambient_light_value;

  // start reading VEML6030 every 5 second, 8 * 5 soft timer (1 timer = 125ms)
  if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_system_soft_timer_id){
      if (VEML6030_timer_count == 0){
          start_i2c = true;
          VEML6030_timer_count++;
      }
      else if(VEML6030_timer_count == 39){
          read_sensor = true;
          VEML6030_timer_count = 0;
      }
      else {
          VEML6030_timer_count++;
      }
  }
  else{
      return;
  }

  switch(currState_VEML6030){
    case VEML6030_IDLE:
      if (start_i2c){
          set_I2C_transfer_target(I2C_TRANSFER_VEML6030);
          VEML6030_start_read_ambient_light_level();
          currState_VEML6030 = VEML6030_WAIT_I2C_READ;
      }
      break;
    case VEML6030_WAIT_I2C_READ:
      if (read_sensor){
          ambient_light_value = VEML6030_read_measured_ambient_light();
          LOG_INFO("Current Ambient Light: %d\r\n", ambient_light_value);
          currState_VEML6030 = VEML6030_IDLE;
      }
      break;
    default:
      break;
  }

}
#endif

