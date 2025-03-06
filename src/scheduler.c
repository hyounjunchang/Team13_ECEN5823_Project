/***********************************************************************
 * @file      scheduler.c
 * @brief     Task Scheduler for Blue Gecko, Keeps state of Si7021
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 20, 2025
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

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#if DEVICE_IS_BLE_SERVER == 1
// Define bit-flags
#define NO_FLAG 0x0
#define BLE_LETIMER0_UF_FLAG 0x1
#define BLE_LETIMER0_COMP1_FLAG 0x2
#define BLE_I2C_TRANSFER_FLAG 0x4

static SI7021_state currState_SI7021 = SI7021_IDLE;
#else
static ble_client_state currState_client = CLIENT_BLE_OFF;
#endif

// edited from Lecture 6 slides
// sets event flag for future use
void set_scheduler_event(scheduler_event event){

#if DEVICE_IS_BLE_SERVER
  sl_status_t sc;
  CORE_DECLARE_IRQ_STATE;
  switch (event){
    case NO_EVENT:
      break;
    // prioritize I2C transfer event first
    case EVENT_I2C_TRANSFER:
     CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
     sc = sl_bt_external_signal(BLE_I2C_TRANSFER_FLAG);
     CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
     if (sc != SL_STATUS_OK){
         LOG_ERROR("Error setting BLE_I2C_TRANSFER_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
     }
     break;
    case EVENT_LETIMER0_COMP1:
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      sc = sl_bt_external_signal(BLE_LETIMER0_COMP1_FLAG);
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting BLE_LETIMER0_COMP1_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    case EVENT_LETIMER0_UF:
      CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
      sc = sl_bt_external_signal(BLE_LETIMER0_UF_FLAG);
      CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting BLE_LETIMER0_UF_FLAG, Error Code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    default:
      break;
  }
#else
  // just to remove compiler warnings
  switch (event){
    default:
      break;
  }
#endif
}

#if DEVICE_IS_BLE_SERVER
SI7021_state get_SI7021_state(){
  return currState_SI7021;
}

void temperature_state_machine(sl_bt_msg_t* evt){
  // only update on external signal (non-bluetooth)
  if (SL_BT_MSG_ID(evt->header) != sl_bt_evt_system_external_signal_id){
      return;
  }

  uint32_t ble_event_flags = evt->data.evt_system_external_signal.extsignals;

  ble_data_struct_t* ble_data_ptr = get_ble_data();
  bool ble_connection_alive = ble_data_ptr->connection_alive;

  switch(currState_SI7021){
    case SI7021_IDLE:
      if ((ble_event_flags & BLE_LETIMER0_UF_FLAG) && ble_connection_alive &&
          ble_data_ptr->ok_to_send_htm_indications) {
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
      if (ble_event_flags & BLE_I2C_TRANSFER_FLAG){
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
      if (ble_event_flags & BLE_I2C_TRANSFER_FLAG){
          // take action and update state
          if (ble_connection_alive){
            // read temperature
            int curr_temperature = SI7021_read_measured_temp();
            //LOG_INFO("Read temperature %iC\r\n", curr_temperature);
            update_temp_meas_gatt_and_send_indication(curr_temperature);
          }
          gpioPowerOff_SI7021();
          currState_SI7021 = SI7021_IDLE;
      }
      break;
    default:
      break;
  }
}
#else
ble_client_state get_client_state(){
  return currState_client;
}

// update client state machine
void client_state_machine(sl_bt_msg_t* evt){

  switch (currState_client){
    case CLIENT_BLE_OFF:
      if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_system_boot_id){
          currState_client = CLIENT_SCANNING;
      }
      break;
    case CLIENT_SCANNING:
      if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_connection_opened_id){
          currState_client = CLIENT_CHECK_GATT_SERVICE;
      }
      break;
    case CLIENT_CHECK_GATT_SERVICE:
      if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_procedure_completed_id){
          sl_bt_evt_gatt_procedure_completed_t gatt_completed = evt->data.evt_gatt_procedure_completed;
          if (gatt_completed.result == SL_STATUS_OK){
              currState_client = CLIENT_CHECK_GATT_CHARACTERSTIC;
          }
      }
      break;
    case CLIENT_CHECK_GATT_CHARACTERSTIC:
      if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_procedure_completed_id){
          sl_bt_evt_gatt_procedure_completed_t gatt_completed = evt->data.evt_gatt_procedure_completed;
          if (gatt_completed.result == SL_STATUS_OK){
              currState_client = CLIENT_SET_GATT_INDICATION;
          }
      }
      break;
    case CLIENT_SET_GATT_INDICATION:
      if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_procedure_completed_id){
          sl_bt_evt_gatt_procedure_completed_t gatt_completed = evt->data.evt_gatt_procedure_completed;
          if (gatt_completed.result == SL_STATUS_OK){
              currState_client = CLIENT_RECEIVE_TEMP_DATA;
          }
      }
      break;
    case CLIENT_RECEIVE_TEMP_DATA:
      if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_connection_closed_id){
          currState_client = CLIENT_SCANNING;
      }
      break;
    default:
      break;
  }
}
#endif
