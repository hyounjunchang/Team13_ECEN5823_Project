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

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

// Define bit-flags
#define NO_FLAG 0x0
#define BLE_LETIMER0_UF_FLAG 0x1
#define BLE_LETIMER0_COMP1_FLAG 0x2
#define BLE_I2C_TRANSFER_FLAG 0x4

static SI7021_state currState_SI7021 = SI7021_IDLE;

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

  switch (event){
    case NO_EVENT:
      break;
    // prioritize I2C transfer event first
    case EVENT_I2C_TRANSFER:
     sc = sl_bt_external_signal(BLE_I2C_TRANSFER_FLAG);
     if (sc != SL_STATUS_OK){
         LOG_ERROR("Error setting BLE_I2C_TRANSFER_FLAG\r\n");
     }
     break;
    case EVENT_LETIMER0_COMP1:
      sc = sl_bt_external_signal(BLE_LETIMER0_COMP1_FLAG);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting BLE_LETIMER0_COMP1_FLAG\r\n");
      }
      break;
    case EVENT_LETIMER0_UF:
      sc = sl_bt_external_signal(BLE_LETIMER0_UF_FLAG);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting BLE_LETIMER0_UF_FLAG\r\n");
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


  // sl_bt_evt_system_external_signal_id DOESN'T TRIGGER THIS!!!
  if (evt->header != 0x30104a0){
      return;
  }

  uint32_t ble_event_flags = evt->data.evt_system_external_signal.extsignals;

  LOG_INFO("curr state: %i\r\n", currState_SI7021);

  switch(currState_SI7021){
    case SI7021_IDLE:
      if (ble_event_flags & BLE_LETIMER0_UF_FLAG){
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
          uint16_t temperature = SI7021_read_measured_temp();
          LOG_INFO("Read temperature %iC\r\n", temperature);
          gpioPowerOff_SI7021();
          currState_SI7021 = SI7021_IDLE;
      }
      break;
    default:
      break;
  }
}
