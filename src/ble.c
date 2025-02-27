/***********************************************************************
 * @file      ble.c
 * @brief     BLE module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Feb 20, 2025
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
#include "lcd.h" // for LCD display

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


// each tick is 0.625ms -> 1/0.625 = 1.6
#define ADVERTISING_INTERVAL(x) (x * 8) / 5
#define AD_INVERTAL_MS_VAL 250


// each tick is 1.25ms --> 1/1.25 = 0.8
#define CONNECTION_INTERVAL(x) (x * 4) / 5
#define CONN_INTERVAL_MS_VAL 75

// BLE private data
ble_data_struct_t ble_data = {.myAddress = {{0}}, .myAddressType = 0,
                              .advertisingSetHandle = 0, .connectionHandle = 0,
                              .connection_alive = false,
                              .ok_to_send_htm_indications = false,
                              .indication_in_flight = false};

// buffer for sending values
uint8_t htm_temperature_buffer[5];
uint8_t *p = &htm_temperature_buffer[0];
uint32_t htm_temperature_flt;
uint8_t flags = 0x00;

int latest_temp = 0;

// Referenced from Lecture 10 slides
void update_temp_meas_gatt_and_send_indication(int temp_in_c){
   sl_status_t sc;

  // convert temperature for bluetooth
   UINT8_TO_BITSTREAM(p, flags); // insert the flags byte
   htm_temperature_flt = INT32_TO_FLOAT(temp_in_c*1000, -3);
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
   if (ble_data.ok_to_send_htm_indications) {
       sc = sl_bt_gatt_server_send_indication(
         ble_data.connectionHandle,
         gattdb_temperature_measurement, // handle from gatt_db.h
         5,
         &htm_temperature_buffer[0] // in IEEE-11073 format
       );
       if (sc != SL_STATUS_OK) {
           LOG_ERROR("Error Sending Indication, Error Code: 0x%x\r\n", (uint16_t)sc);
           ble_data.indication_in_flight = false;
       }
       else {
           //Set indication_in_flight flag
           ble_data.indication_in_flight = true;
           //LOG_INFO("indication in flight\r\n");
       }
   }

   // print temperature on lcd
   displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", temp_in_c);
   // update latest temperature value
   latest_temp = temp_in_c;
}


ble_data_struct_t* get_ble_data(){
  return &ble_data;
}

// from lecture 10
void handle_ble_event(sl_bt_msg_t* evt){

  sl_status_t sc;
  sl_bt_evt_connection_opened_t bt_conn_open;
  //sl_bt_evt_connection_parameters_t bt_conn_param;

  // for updating
  sl_bt_evt_gatt_server_characteristic_status_t gatt_server_char_status;
  uint16_t characteristic;

  switch (SL_BT_MSG_ID(evt->header)) {
    // ******************************************************
    // Events common to both Servers and Clients
    // ******************************************************
    // --------------------------------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack API commands before receiving this boot event!
    // Including starting BT stack soft timers!
    // --------------------------------------------------------

    // Indicates that the device has started and the radio is ready
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

      // Start LCD Display
      displayInit();

      uint16_t ble_addr[6]; // due to printf errors
      uint8_t* addr = (uint8_t*)&ble_data.myAddress;
      // storing address as uint16_t arr to print address
      for (int i = 0; i < 6; i++){
          ble_addr[i] = (uint16_t)(*addr);
          addr++;
      }

      // Write Info to LCD Display
      displayPrintf(DISPLAY_ROW_NAME, "Server");
      displayPrintf(DISPLAY_ROW_BTADDR, "%02X:%02X:%02X:%02X:%02X:%02X",
                    ble_addr[0], ble_addr[1], ble_addr[2],
                    ble_addr[3], ble_addr[4], ble_addr[5]);
      displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
      displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A6");

      break;
    // Indicates that a new connection was opened
    case sl_bt_evt_connection_opened_id:
      bt_conn_open = evt->data.evt_connection_opened;

      // stop advertising once connection complete
      sc = sl_bt_advertiser_stop(ble_data.advertisingSetHandle);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error stopping Bluetooth advertising, Error code: 0x%x\r\n", (uint16_t)sc);
      }

      // request sl_bt_connection parameter
      sc = sl_bt_connection_set_parameters(bt_conn_open.connection,
                                           CONNECTION_INTERVAL(75), // 75ms
                                           CONNECTION_INTERVAL(75), // 75ms
                                           1, // up to 1 missed, this causes some errors with
                                                  //the Si Connect app with some values
                                           1600, // 16 second timeout, for weak bluetooth signals
                                           0, // default for connection event min/maxlength
                                           0xffff
                                           );
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error requesting Bluetooth connection parameters, Error code: 0x%x\r\n", (uint16_t)sc);
      }

      ble_data.connectionHandle = bt_conn_open.connection;
      ble_data.connection_alive = true;

      // display that server in connected mode
      displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
      break;
    // sl_bt_evt_connection_closed_id
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

      // update states
      ble_data.ok_to_send_htm_indications = 0;
      ble_data.indication_in_flight = false;
      ble_data.connection_alive = false;

      // display that server in advertising mode
      displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
      // clear temperature on lcd
      displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
      break;
    // Triggered whenever the connection parameters are changed and at any
    // time a connection is established
    case sl_bt_evt_connection_parameters_id:
      // Uncomment to log connection parameters
      /*
      bt_conn_param = evt->data.evt_connection_parameters;
      LOG_INFO("Connection parameter changed\r\n");
      LOG_INFO("Interval(1.25ms): %d\r\n", bt_conn_param.interval);
      LOG_INFO("Latency(num_skip_allowed): %d\r\n", bt_conn_param.latency);
      LOG_INFO("Timeout(10ms): %d\r\n", bt_conn_param.timeout);
      */
      break;
    case sl_bt_evt_system_external_signal_id:
      // si7021 state is kept in scheduler.c, so no action is taken here.
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

      // characteristic for CCCD change
      characteristic = gatt_server_char_status.characteristic;

      // CCCD changed
      if(gatt_server_char_status.status_flags & sl_bt_gatt_server_client_config){
          if (characteristic == gattdb_temperature_measurement){ // handle from gatt_db.h

              // indication flag
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_indication){
                ble_data.ok_to_send_htm_indications = true;
                // lcd temperature only updated when temperature measured
              }
              else{
                ble_data.ok_to_send_htm_indications = false;
                // clear temperature on lcd
                displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
              }

              //LOG_INFO("HTM_INDICATION CHANGED! Value: %x\r\n", gatt_server_char_status.client_config_flags);
          }
      }
      // GATT indication received
      if(gatt_server_char_status.status_flags & sl_bt_gatt_server_confirmation){
          ble_data.indication_in_flight = false;
          //LOG_INFO("GATT indication received\r\n");
      }
      break;
    // Indicates confirmation from the remote GATT client has not been
    // received within 30 seconds after an indication was sent
    case sl_bt_evt_gatt_server_indication_timeout_id:
      // LOG_INFO("Received Indication Timeout\r\n");
      // send indication
      if (ble_data.ok_to_send_htm_indications) {
          sc = sl_bt_gatt_server_send_indication(
            ble_data.connectionHandle,
            gattdb_temperature_measurement, // handle from gatt_db.h
            5,
            p // in IEEE-11073 format
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
