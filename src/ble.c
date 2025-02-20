/***********************************************************************
 * @file      ble.c
 * @brief     BLE module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 19, 2025
 *
 * @resources Lecture 10
 *
 *
 *
 */

#include "ble.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"

// BLE private data
ble_data_struct_t ble_data;


ble_data_struct_t* get_ble_data(){
  return &ble_data;
}

// from lecture 10
void handle_ble_event(sl_bt_msg_t* evt){

  switch (SL_BT_MSG_ID(evt->header)) {
    // ******************************************************
    // Events common to both Servers and Clients
    // ******************************************************
    // --------------------------------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack API commands before receiving this boot event!
    // Including starting BT stack soft timers!
    // --------------------------------------------------------
    case sl_bt_evt_system_boot_id:
      // handle boot event
      break;
    case sl_bt_evt_connection_opened_id:
      // handle open event
      break;
    case sl_bt_evt_connection_closed_id:
      // handle close event
      break;
    // ******************************************************
    // Events for Server
    // ******************************************************
    // ******************************************************
    // Events for Client
    // ******************************************************
    default:
      break;

  // more case statements to handle other BT events
  }

}
