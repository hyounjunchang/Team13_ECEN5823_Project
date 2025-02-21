/***********************************************************************
 * @file      scheduler.c
 * @brief     Task Scheduler for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 13, 2025
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

// Define bit-flags
#define NO_FLAG 0x0
#define BLE_LETIMER0_UF_FLAG 0x1
#define BLE_LETIMER0_COMP1_FLAG 0x2
#define BLE_I2C_TRANSFER_FLAG 0x4

static SI7021_state currState_SI7021 = SI7021_IDLE;

uint8_t htm_temperature_buffer[5];
uint8_t *p = &htm_temperature_buffer[0];
uint32_t htm_temperature_flt;
uint8_t flags = 0x00;

uint8_t* get_htm_temperature_buffer_ptr(){
  return p;
}


// edited from Lecture 6 slides
// returns a scheduler_event among one of the events available
// clears returned event flag
// no longer used
/*
scheduler_event getNextEvent(){
  CORE_DECLARE_IRQ_STATE;
  scheduler_event next_event = NO_EVENT;

  // clear flag and return event
  // This is different from register flags!!!
  if (active_events.flag_0){
      if (active_events.flag_0 & BLE_I2C_TRANSFER_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.flag_0 &= ~BLE_I2C_TRANSFER_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return EVENT_I2C_TRANSFER;
      }
      else if (active_events.flag_0 & BLE_LETIMER0_COMP1_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.flag_0 &= ~BLE_LETIMER0_COMP1_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return EVENT_LETIMER0_COMP1;
      }
      else if (active_events.flag_0 & BLE_LETIMER0_UF_FLAG){
          CORE_ENTER_CRITICAL(); // NVIC IRQs are disabled
          active_events.flag_0 &= ~BLE_LETIMER0_UF_FLAG;
          CORE_EXIT_CRITICAL(); // re-enable NVIC interrupts
          return EVENT_LETIMER0_UF;
      }
  }

  return next_event;
}
*/

// edited from Lecture 6 slides
// sets event flag for future use
void set_scheduler_event(scheduler_event event){
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
}

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
  sl_status_t sc;

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
            uint16_t curr_temperature = SI7021_read_measured_temp();
            //LOG_INFO("Read temperature %iC\r\n", curr_temperature);

            // convert temperature for bluetooth
            UINT8_TO_BITSTREAM(p, flags); // insert the flags byte
            htm_temperature_flt = INT32_TO_FLOAT(curr_temperature*1000, -3);
            // insert the temperature measurement
            UINT32_TO_BITSTREAM(p, htm_temperature_flt);

            // write to gatt_db
            sc = sl_bt_gatt_server_write_attribute_value(
                  gattdb_temperature_measurement, // handle from gatt_db.h
                  0, // offset
                  5, // length
                  &htm_temperature_buffer[0] // in IEEE-11073 format
            );
            if (sc != SL_STATUS_OK) {
                LOG_ERROR("Error setting GATT for temp measurement, Error Code: 0x%x\r\n", (uint16_t)sc);
            }
            // send indication
            if (ble_data_ptr->ok_to_send_htm_indications) {
                sc = sl_bt_gatt_server_send_indication(
                  ble_data_ptr->connectionHandle,
                  gattdb_temperature_measurement, // handle from gatt_db.h
                  5,
                  &htm_temperature_buffer[0] // in IEEE-11073 format
                );
                if (sc != SL_STATUS_OK) {
                    LOG_ERROR("Error Sending Indication, Error Code: 0x%x\r\n", (uint16_t)sc);
                }
                else {
                    //Set indication_in_flight flag
                    ble_data_ptr->indication_in_flight = true;
                    //LOG_INFO("indication in flight\r\n");
                }
            } // if
          }
          gpioPowerOff_SI7021();
          currState_SI7021 = SI7021_IDLE;
      }
      break;
    default:
      break;
  }
}
