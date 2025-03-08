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
#include "ble_device_type.h"
#include "ble.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"

#include "scheduler.h"
#include "lcd.h" // for LCD display

#include <stdint.h>
#include <math.h>

// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


// each tick is 0.625ms -> 1/0.625 = 1.6
#define ADVERTISING_INTERVAL(x) (x * 8) / 5
#define AD_INVERTAL_MS_VAL 250

// each tick is 1.25ms --> 1/1.25 = 0.8
#define CONNECTION_INTERVAL(x) (x * 4) / 5
#define CONN_INTERVAL_MS_VAL 75

// 300ms latency
#define SLAVE_LATENCY_MS_VAL 300
#define SLAVE_LATENCY_INTERVALS 4 // 300/75

#define SUPERVISION_TIMEOUT_MS_VAL (1 + SLAVE_LATENCY_INTERVALS) * (CONN_INTERVAL_MS_VAL * 2) + CONN_INTERVAL_MS_VAL
// divide by 10 since each value is 10ms
#define SUPERVISION_TIMEOUT (SUPERVISION_TIMEOUT_MS_VAL / 10) + 1 // +1 to meet supervision req

// Used by Client Only

// each tick is 0.625 ms -> 1/0.625 = 1.6
#define SCANNING_INTERVAL(x) (x * 8) / 5
#define SCAN_INTERVAL_MS_VAL 50
#define SCAN_WINDOW_MS_VAL 25


// BLE private data
ble_data_struct_t ble_data = {.myAddress = {{0}}, .myAddressType = 0,
                              .advertisingSetHandle = 0, .connectionHandle = 0,
                              .connection_alive = false,
                              .ok_to_send_htm_indications = false,
                              .indication_in_flight = false,
                              .gatt_service_found = false,
                              .gatt_characteristic_found = false,
                              .htmServiceHandle = 0,
                              .tempMeasHandle = 0};


// buffer for sending values for server
uint8_t htm_temperature_buffer[5];
uint8_t *p = &htm_temperature_buffer[0];
uint32_t htm_temperature_flt;
uint8_t flags = 0x00;

// for client, uuid in little-endian format
uint8_t uuid_health_thermometer[2] = {0x09, 0x18};
uint8_t uuid_temp_measurement[2] = {0x1C, 0x2A};

int latest_temp = 0;

#if DEVICE_IS_BLE_SERVER
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
#else
// Private function, original from Dan Walkes. I fixed a sign extension bug.
// We'll need this for Client A7 assignment to convert health thermometer
// indications back to an integer. Convert IEEE-11073 32-bit float to signed integer.
static int32_t FLOAT_TO_INT32(const uint8_t *buffer_ptr)
{
  uint8_t signByte = 0;
  int32_t mantissa;
  // input data format is:
  // [0] = flags byte, bit[0] = 0 -> Celsius; =1 -> Fahrenheit
  // [3][2][1] = mantissa (2's complement)
  // [4] = exponent (2's complement)
  // BT buffer_ptr[0] has the flags byte
  int8_t exponent = (int8_t)buffer_ptr[4];
  // sign extend the mantissa value if the mantissa is negative
  if (buffer_ptr[3] & 0x80) { // msb of [3] is the sign of the mantissa
  signByte = 0xFF;
  }
  mantissa = (int32_t) (buffer_ptr[1] << 0) |
  (buffer_ptr[2] << 8) |
  (buffer_ptr[3] << 16) |
  (signByte << 24) ;
  // value = 10^exponent * mantissa, pow() returns a double type
  return (int32_t) (pow(10, exponent) * mantissa);
} // FLOAT_TO_INT32
#endif


ble_data_struct_t* get_ble_data(){
  return &ble_data;
}

// from lecture 10
void handle_ble_event(sl_bt_msg_t* evt){

  sl_status_t sc;
  sl_bt_evt_connection_opened_t bt_conn_open;
  //sl_bt_evt_connection_parameters_t bt_conn_param;

#if DEVICE_IS_BLE_SERVER // for server states
  sl_bt_evt_gatt_server_characteristic_status_t gatt_server_char_status;
  uint16_t characteristic;
#else
  sl_bt_evt_scanner_legacy_advertisement_report_t scan_report;
  sl_bt_evt_gatt_procedure_completed_t gatt_completed;
  sl_bt_evt_gatt_service_t gatt_service;
  sl_bt_evt_gatt_characteristic_t gatt_characteristic;
  uint8_t *val_ptr;
#endif

#if DEVICE_IS_BLE_SERVER
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
      displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A7");

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
                                           CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL), // 75ms
                                           CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL), // 75ms
                                           SLAVE_LATENCY_INTERVALS, // see declaration above
                                           SUPERVISION_TIMEOUT, // see declaration above
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
      // update states
      ble_data.ok_to_send_htm_indications = false;
      ble_data.indication_in_flight = false;
      ble_data.connection_alive = false;

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
    // Bluetooth soft timer interrupt (1 second)
    case sl_bt_evt_system_soft_timer_id:
      displayUpdate(); // prevent charge buildup within the Liquid Crystal Cells
      break;
    default:
      break;
  }
#else
  switch (SL_BT_MSG_ID(evt->header)) {
     // Indicates that the device has started and the radio is ready
     case sl_bt_evt_system_boot_id:
       // handle boot event
       sc = sl_bt_system_get_identity_address(&ble_data.myAddress, &ble_data.myAddressType);
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error getting Bluetooth identity address, Error code: 0x%x\r\n", (uint16_t)sc);
       }

       // start scanning for server
       sc = sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_passive,
                                         SCANNING_INTERVAL(SCAN_INTERVAL_MS_VAL),
                                         SCANNING_INTERVAL(SCAN_WINDOW_MS_VAL)); // passive scan, 50ms interval, 25ms window
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error setting scanner parameters, Error code: 0x%x\r\n", (uint16_t)sc);
       }

       // default connection parameters
       sc = sl_bt_connection_set_default_parameters(CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL), // 75ms
                                                    CONNECTION_INTERVAL(CONN_INTERVAL_MS_VAL), // 75ms
                                                    SLAVE_LATENCY_INTERVALS, // 4 skipped
                                                    SUPERVISION_TIMEOUT,
                                                    0, 4); // Connection event length: min = 0ms, max = 4 * 0.625ms
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error setting connection default parameters, Error code: 0x%x\r\n", (uint16_t)sc);
       }

       // start scanning for device
       sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m_and_coded,
                                sl_bt_scanner_discover_generic); // limited and general
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error starting Bluetooth scanner, Error code: 0x%x\r\n", (uint16_t)sc);
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
       displayPrintf(DISPLAY_ROW_NAME, "Client");
       displayPrintf(DISPLAY_ROW_BTADDR, "%02X:%02X:%02X:%02X:%02X:%02X",
                     ble_addr[0], ble_addr[1], ble_addr[2],
                     ble_addr[3], ble_addr[4], ble_addr[5]);
       displayPrintf(DISPLAY_ROW_CONNECTION, "Discovering");
       displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A7");
       break;
     // Received for advertising or scan response packets generated by: sl_bt_scanner_start()
     case sl_bt_evt_scanner_legacy_advertisement_report_id:
       scan_report = evt->data.evt_scanner_legacy_advertisement_report;

       bool address_match = true;
       // start connection if address matches server
       uint8_t* scanned_addr = (uint8_t*)&(scan_report.address);
       uint8_t* server_addr = (uint8_t*)&(SERVER_BT_ADDRESS.addr);
       for (int i = 0; i < 6; i++){
           if (*scanned_addr != *server_addr){
               address_match = false;
           }
           scanned_addr++;
           server_addr++;
       }

       if (address_match){
           LOG_INFO("address matched\r\n");
           sc = sl_bt_scanner_stop();
           if (sc != SL_STATUS_OK){
               LOG_ERROR("Error stopping Bluetooth scanner, Error code: 0x%x\r\n", (uint16_t)sc);
           }
           sc = sl_bt_connection_open(scan_report.address,
                                      scan_report.address_type,
                                      sl_bt_gap_phy_1m,
                                      &ble_data.connectionHandle);
           if (sc != SL_STATUS_OK){
               LOG_ERROR("Error opening Bluetooth connection, Error code: 0x%x\r\n", (uint16_t)sc);
           }
       }
       break;
     // Indicates that a new connection was opened
     case sl_bt_evt_connection_opened_id:
       LOG_INFO("BLE connection opened\r\n");

       // store ble connection handle
       bt_conn_open = evt->data.evt_connection_opened;

       ble_data.connectionHandle = bt_conn_open.connection;
       ble_data.connection_alive = true;
       // discover health thermometer service
       sc = sl_bt_gatt_discover_primary_services_by_uuid(ble_data.connectionHandle,
                                                         2,
                                                         &uuid_health_thermometer[0]);
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error discovering GATT services, Error code: 0x%x\r\n", (uint16_t)sc);
       }
       // display that server in connected mode
       displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
       break;
     // sl_bt_evt_connection_closed_id
     case sl_bt_evt_connection_closed_id:
       // update states
       ble_data.connection_alive = false;
       ble_data.gatt_service_found = false;
       ble_data.gatt_characteristic_found = false;
       ble_data.ok_to_send_htm_indications = false;

       // start scanning for device
       sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m_and_coded,
                                sl_bt_scanner_discover_generic); // limited and general
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error starting Bluetooth scanner, Error code: 0x%x\r\n", (uint16_t)sc);
       }
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
     // GATT procedure (find/set characteristic) completed, indication of temperature
     case sl_bt_evt_gatt_procedure_completed_id:
       LOG_INFO("BLE GATT procedure completed\r\n");

       gatt_completed = evt->data.evt_gatt_procedure_completed;
       sc = gatt_completed.result;
       ble_client_state client_state = get_client_state();


       // GATT procedure successful
       if (sc == SL_STATUS_OK){
           switch(client_state){
             case CLIENT_CHECK_GATT_SERVICE:
               ble_data.gatt_service_found = true;
               sc = sl_bt_gatt_discover_characteristics_by_uuid(ble_data.connectionHandle,
                                                                ble_data.htmServiceHandle,
                                                                2,
                                                                &uuid_temp_measurement[0]);
               if (sc != SL_STATUS_OK){
                   LOG_ERROR("Error discovering GATT characteristic, Error code: 0x%x\r\n", (uint16_t)sc);
               }
               break;
             case CLIENT_CHECK_GATT_CHARACTERSTIC:
               ble_data.gatt_characteristic_found = true;
               sc = sl_bt_gatt_set_characteristic_notification(ble_data.connectionHandle,
                                                               ble_data.tempMeasHandle,
                                                               sl_bt_gatt_indication);
               if (sc != SL_STATUS_OK){
                   LOG_ERROR("Error setting GATT indication, Error code: 0x%x\r\n", (uint16_t)sc);
               }
               break;
             case CLIENT_SET_GATT_INDICATION:
               ble_data.ok_to_send_htm_indications = true;
               break;
             default:
               break;
           }
       }
       // GATT procedure failed
       else{
           switch(client_state){
             case CLIENT_CHECK_GATT_SERVICE:
               LOG_ERROR("Error finding GATT service, Error code: 0x%x\r\n", (uint16_t)sc);
               break;
             case CLIENT_CHECK_GATT_CHARACTERSTIC:
               LOG_ERROR("Error finding GATT characteristic, Error code: 0x%x\r\n", (uint16_t)sc);
               break;
             case CLIENT_SET_GATT_INDICATION:
               LOG_ERROR("Error setting GATT indication, Error code: 0x%x\r\n", (uint16_t)sc);
               break;
             default:
               break;
           }
       }
       break;
     // GATT service discovered
     case sl_bt_evt_gatt_service_id:
       LOG_INFO("BLE GATT service received\r\n");
       // store GATT service handle
       gatt_service = evt->data.evt_gatt_service;
       ble_data.htmServiceHandle = gatt_service.service;
       break;
     // GATT characteristic discovered in the server
     case sl_bt_evt_gatt_characteristic_id:
       LOG_INFO("BLE GATT characteristic received\r\n");
       // store GATT characteristic handle
       gatt_characteristic = evt->data.evt_gatt_characteristic;
       ble_data.tempMeasHandle = gatt_characteristic.characteristic;
       break;
     // Indication or Notification has been received
     case sl_bt_evt_gatt_characteristic_value_id:
       val_ptr = &(evt->data.evt_gatt_characteristic_value.value.data[0]);

       /*
       uint8_t temp[5];
       temp[0] = evt->data.evt_gatt_characteristic_value.value.data[0];
       temp[1] = evt->data.evt_gatt_characteristic_value.value.data[1];
       temp[2] = evt->data.evt_gatt_characteristic_value.value.data[2];
       temp[3] = evt->data.evt_gatt_characteristic_value.value.data[3];
       temp[4] = evt->data.evt_gatt_characteristic_value.value.data[4];
       LOG_INFO("Arr values: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\r\n", temp[0], temp[1], temp[2], temp[3], temp[4]);
       */

       int32_t temperature = FLOAT_TO_INT32(val_ptr);
       LOG_INFO("Received Temperature = %d\r\n", temperature);

       // send confirmation for temperature indication
       sc = sl_bt_gatt_send_characteristic_confirmation(ble_data.connectionHandle);
       if (sc != SL_STATUS_OK){
           LOG_ERROR("Error sending GATT indication confirmation, Error code: 0x%x\r\n", (uint16_t)sc);
       }

       // display temperature displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", temp_in_c);
       break;
     // external Bluetooth signals
     case sl_bt_evt_system_external_signal_id:
       break;
     // Bluetooth soft timer interrupt (1 second)
     case sl_bt_evt_system_soft_timer_id:
       displayUpdate(); // prevent charge buildup within the Liquid Crystal Cells
       break;
     default:
       break;
   }

#endif

}
