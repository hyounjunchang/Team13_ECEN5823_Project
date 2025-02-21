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

#include "scheduler.h"

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
#define SUPERVISION_TIMEOUT (SUPERVISION_TIMEOUT_MS_VAL / 10) + 1 // +1 to meet supervision req

// BLE private data
ble_data_struct_t ble_data = {.myAddress = {{0}}, .myAddressType = 0,
                              .advertisingSetHandle = 0, .connectionHandle = 0,
                              .connection_alive = false,
                              .ok_to_send_htm_indications = false,
                              .indication_in_flight = false};

// add states for advertising, scanning, measuring temp data, etcs

ble_data_struct_t* get_ble_data(){
  return &ble_data;
}

// from lecture 10
void handle_ble_event(sl_bt_msg_t* evt){

  sl_status_t sc;
  sl_bt_evt_connection_opened_t bt_conn_open;
  sl_bt_evt_connection_parameters_t bt_conn_param;

  // for updating
  sl_bt_evt_gatt_server_characteristic_status_t gatt_server_char_status;
  uint16_t characteristic;
  uint16_t client_config_flags;
  uint16_t client_config_handle;

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
      LOG_INFO("connection request incoming\r\n");

      bt_conn_open = evt->data.evt_connection_opened;

      // handle open event
      sc = sl_bt_advertiser_stop(ble_data.advertisingSetHandle);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error stopping Bluetooth advertising, Error code: 0x%x\r\n", (uint16_t)sc);
      }


      // request sl_bt_connection parameter
      sc = sl_bt_connection_set_parameters(bt_conn_open.connection,
                                           CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL),
                                           CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL),
                                           SLAVE_LATENCY_INTERVALS,
                                           SUPERVISION_TIMEOUT,
                                           0, // default for connection event min/maxlength
                                           0xffff
                                           );
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error requesting Bluetooth connection parameters, Error code: 0x%x\r\n", (uint16_t)sc);
      }


      ble_data.connectionHandle = bt_conn_open.connection;
      ble_data.connection_alive = true;
      break;
    case sl_bt_evt_connection_closed_id:
      LOG_INFO("Connection closed\r\n");

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

      // update states
      ble_data.ok_to_send_htm_indications = 0;
      ble_data.indication_in_flight = false;
      ble_data.connection_alive = false;
      break;
    case sl_bt_evt_connection_parameters_id:
      bt_conn_param = evt->data.evt_connection_parameters;
      LOG_INFO("Connection parameter changed\r\n");
      LOG_INFO("Interval(1.25ms): %d\r\n", bt_conn_param.interval);
      LOG_INFO("Latency(num_skip_allowed): %d\r\n", bt_conn_param.latency);
      LOG_INFO("Timeout(10ms): %d\r\n", bt_conn_param.timeout);
      break;
    case sl_bt_evt_system_external_signal_id:
      break;
    // ******************************************************
    // Events for Server
    // ******************************************************
    // Indicates either: A local Client Characteristic Configuration descriptor (CCCD)
    // was changed by the remote GATT client, or
    // That a confirmation from the remote GATT Client was received upon a successful
    // reception of the indication I.e. we sent an indication from our server to the
    // client with sl_bt_gatt_server_send_indication()
    case sl_bt_evt_gatt_server_characteristic_status_id:
      // data received
      gatt_server_char_status = evt->data.evt_gatt_server_characteristic_status;

      // config flag to be used for CCCD change
      characteristic = gatt_server_char_status.characteristic;

      //client_config_flags = gatt_server_char_status.client_config_flags;
      //client_config_handle = gatt_server_char_status.client_config;

      //LOG_INFO("Char_status received, characteristic %x\r\n", characteristic);
      //LOG_INFO("Char_status received, client_config_handle %x\r\n", client_config_handle);
      //LOG_INFO("Char_status received, client_config_flags %x\r\n", client_config_flags);

      // CCCD changed
      if(gatt_server_char_status.status_flags & sl_bt_gatt_server_client_config){
          if (characteristic == gattdb_temperature_measurement){ // handle from gatt_db.h

              // indication flag
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_indication){
                ble_data.ok_to_send_htm_indications = true;
              }
              else{
                ble_data.ok_to_send_htm_indications = false;
              }

              LOG_INFO("HTM_INDICATION CHANGED! Value: %x\r\n", gatt_server_char_status.client_config_flags);
          }
      }
      // GATT indication received
      if(gatt_server_char_status.status_flags & sl_bt_gatt_server_confirmation){
          ble_data.indication_in_flight = false;
      }

      break;
    // Indicates confirmation from the remote GATT client has not been
    // received within 30 seconds after an indication was sent
    case sl_bt_evt_gatt_server_indication_timeout_id:
      // resend data
      LOG_INFO("Received Indication Timeout\r\n");

      uint8_t* buff_ptr = get_htm_temperature_buffer_ptr();

      // send indication
      if (ble_data.ok_to_send_htm_indications) {
          sc = sl_bt_gatt_server_send_indication(
            ble_data.connectionHandle,
            gattdb_temperature_measurement, // handle from gatt_db.h
            5,
            buff_ptr // in IEEE-11073 format
          );
          if (sc != SL_STATUS_OK) {
              LOG_ERROR("Error Sending Indication, Error Code: 0x%x\r\n", (uint16_t)sc);
              ble_data.indication_in_flight = false;
          }
          else{
              ble_data.indication_in_flight = true;
          }
      }



      break;
    // ******************************************************
    // Events for Client
    // ******************************************************
    default:
      break;

  // more case statements to handle other BT events
  }

}
