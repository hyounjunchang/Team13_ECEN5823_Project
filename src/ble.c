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

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


// each tick is 0.625ms -> 1/0.625 = 1.6
#define ADVERTISING_INTERVAL(x) (x * 8) / 5
#define AD_INVERTAL_MS_VAL 250


// each tick is 1.25ms --> 1/1.25 = 0.8
#define CONNECTION_INTERVAL(x) (x * 4) / 5
#define CONN_INTERVAL_MS_VAL 75


// 75ms latency, so slave latency = (4 - 1) intervals can be skipped
#define SLAVE_LATENCY_MS_VAL 300
#define SLAVE_LATENCY_INTERVALS (SLAVE_LATENCY_MS_VAL / CONN_INTERVAL_MS_VAL) - 1

#define SUPERVISION_TIMEOUT_MS_VAL (1 + SLAVE_LATENCY_INTERVALS) * (CONN_INTERVAL_MS_VAL * 2)
#define SUPERVISION_TIMEOUT SUPERVISION_TIMEOUT_MS_VAL / 10

// BLE private data
ble_data_struct_t ble_data = {.myAddress = {{0}}, .myAddressType = 0,
                              .advertisingSetHandle = 0, .connectionHandle = 0,
                              .connection_alive = false,
                              .indication_temp_meas = false,
                              .indication_in_flight = false}; // initialize with all zeros

// add states for advertising, scanning, measuring temp data, etcs

ble_data_struct_t* get_ble_data(){
  return &ble_data;
}

// from lecture 10
void handle_ble_event(sl_bt_msg_t* evt){

  sl_status_t sc;
  sl_bt_evt_connection_opened_t bt_conn_open;

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
      sc = sl_bt_system_get_identity_address(&ble_data.myAddress, &ble_data.myAddressType);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error getting Bluetooth identity address, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      sc = sl_bt_advertiser_create_set(&ble_data.advertisingSetHandle);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error creating Bluetooth advertiser set, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      // 250ms min/max (
      sc = sl_bt_advertiser_set_timing(ble_data.advertisingSetHandle,
                                       ADVERTISING_INTERVAL(AD_INVERTAL_MS_VAL),
                                       ADVERTISING_INTERVAL(AD_INVERTAL_MS_VAL),
                                       0, 0);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error setting Bluetooth advertiser timing, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      sc = sl_bt_legacy_advertiser_generate_data(ble_data.advertisingSetHandle, \
                                                 sl_bt_advertiser_general_discoverable);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error generating Bluetooth advertiser data, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      sc = sl_bt_legacy_advertiser_start(ble_data.advertisingSetHandle, \
                                         sl_bt_advertiser_connectable_scannable);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error starting Bluetooth advertising, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    case sl_bt_evt_connection_opened_id:
      bt_conn_open = evt->data.evt_connection_opened;

      // handle open event
      sc = sl_bt_advertiser_stop(ble_data.advertisingSetHandle);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error stopping Bluetooth advertising, Error code: 0x%x\r\n", (uint16_t)sc);
      }

      // UPDATE LATER!
      sc = sl_bt_connection_set_parameters(bt_conn_open.connection,
                                           CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL),
                                           CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL),
                                           SLAVE_LATENCY_INTERVALS,
                                           SUPERVISION_TIMEOUT,
                                           0, // default for connection event min/maxlength
                                           0
                                           );
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error setting Bluetooth connection parameters, Error code: 0x%x\r\n", (uint16_t)sc);
      }

      ble_data.connectionHandle = bt_conn_open.connection;
      ble_data.connection_alive = true;
      break;
    case sl_bt_evt_connection_closed_id:
      // handle close event
      sc = sl_bt_legacy_advertiser_generate_data(ble_data.advertisingSetHandle, \
                                                 sl_bt_advertiser_general_discoverable);
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error generating Bluetooth advertiser data, Error code: 0x%x\r\n",(uint16_t)sc);
      }

      sc = sl_bt_legacy_advertiser_start(ble_data.advertisingSetHandle, \
                                         sl_bt_advertiser_connectable_scannable);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error starting Bluetooth advertising, Error code: 0x%x\r\n", (uint16_t)sc);
      }

      ble_data.connection_alive = false;
      break;
    case sl_bt_evt_connection_parameters_id:
      break;
    case sl_bt_evt_system_external_signal_id:
      break;
    // ******************************************************
    // Events for Server
    // ******************************************************
    case sl_bt_evt_gatt_server_characteristic_status_id:

      break;
    // Indicates confirmation from the remote GATT client has not been
    // received within 30 seconds after an indication was sent
    case sl_bt_evt_gatt_server_indication_timeout_id:
      ble_data.indication_in_flight = true;
      break;
    // ******************************************************
    // Events for Client
    // ******************************************************
    default:
      break;

  // more case statements to handle other BT events
  }

}
