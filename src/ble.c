/***********************************************************************
 * @file      ble.c
 * @brief     BLE module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 19, 2025
 *
 * @resources
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

void handle_ble_event(sl_bt_msg_t* evt){

}
