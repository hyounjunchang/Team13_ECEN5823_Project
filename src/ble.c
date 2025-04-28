/***********************************************************************
 * @file      ble.c
 * @brief     BLE module for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Mar 20, 2025
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

#include "gpio.h"

#include "src/em_adc.h"
#include "adc.h"

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


// struct for notification queue
typedef struct{
  uint16_t attribute;
  uint16_t offset;
  size_t value_len;
  uint8_t* value;
} ble_notification_struct_t;


// BLE private data
ble_data_struct_t ble_data = {.myAddress = {{0}}, .myAddressType = 0,
                              .advertisingSetHandle = 0, .connectionHandle = 0,
                              .ok_to_send_htm_notifications = false,
                              .ok_to_send_amb_light_notifications = false,
                              .ok_to_send_sound_level_notifications = false,
                              .ok_to_send_occupied_notifications = false,
                              .passkey_received = false,
                              .is_bonded = false};


// buffers for server
#if DEVICE_IS_BLE_SERVER
// buffer for sending values for server
uint8_t htm_temperature_buffer[5];
uint8_t *htm_temperature_ptr = &htm_temperature_buffer[0];
uint32_t htm_temperature_flt;
uint8_t flags = 0x00;

uint8_t PB0_pressed = 0;
uint8_t *PB0_pressed_ptr = &PB0_pressed;

// sound detector sensor
char sound_level_str[6] = {0};
char* sound_ptr = &sound_level_str[0];
uint32_t sound_level_mv;

// ambient light sensor
uint16_t amb_light_val;
uint16_t* amb_light_ptr = &amb_light_val;

// space occupied
bool space_occupied = false;
char occupied_str[10] = {0}; // Available or Occupied
char* occupied_ptr = &occupied_str[0];

uint32_t* getSoundLevelptr(){
  return &sound_level_mv;
}

// for indications queue
ble_notification_struct_t notif_to_send;

// storing last values
int latest_temp = 0;

#define QUEUE_DEPTH 10
// to send indications via timer
uint8_t lazy_timer_count = 0;
#endif

#if DEVICE_IS_BLE_SERVER
#define FROM_INPUT true
#define FROM_QUEUE false
/*
// sends indication, returns false if not able to, adds it to queue, if indication enabled
// indication: pointer to indication data
// from_input: send data from input, otherwise it is sent from queue
 * return value: true if indication sent, false if added to queue or no indication sent
 */
bool send_notification(ble_notification_struct_t *notification){
  sl_status_t sc;
  // do not send notification if not set
  /*
  if (notification->attribute == gattdb_temperature_measurement &&
      !ble_data.ok_to_send_htm_notifications){
      return false;
  }
  */
  if (notification->attribute == gattdb_illuminance &&
      !ble_data.ok_to_send_amb_light_notifications){
      return false;
  }
  else if (notification->attribute == gattdb_audio_input_description &&
      !ble_data.ok_to_send_sound_level_notifications){
      return false;
  }
  else if (notification->attribute == gattdb_space_occupied &&
      !ble_data.ok_to_send_occupied_notifications){
      return false;
  }

  // send notification
  sc = sl_bt_gatt_server_notify_all(
      notification->attribute, // from autogen/gattdb.h
      notification->value_len,
      notification->value);
  if (sc != SL_STATUS_OK) {
      LOG_ERROR("Error sending GATT notification, Error Code: 0x%x\r\n", (uint16_t)sc);
      return false;
  }
  return true;
}

// dummy function, since temp sensor is disabled
void update_temp_meas_gatt_and_send_notification(int temp_in_c){
  latest_temp = temp_in_c;
}

/*
// Referenced from Lecture 10 slides
void update_temp_meas_gatt_and_send_notification(int temp_in_c){
  sl_status_t sc;
  htm_temperature_ptr = &htm_temperature_buffer[0]; // reset pointer, otherwise causes segmentation fault

  // convert temperature for bluetooth
  UINT8_TO_BITSTREAM(htm_temperature_ptr, flags); // insert the flags byte
  htm_temperature_flt = INT32_TO_FLOAT(temp_in_c*1000, -3);
  // insert the temperature measurement
  UINT32_TO_BITSTREAM(htm_temperature_ptr, htm_temperature_flt);

  // write to gatt_db
  sc = sl_bt_gatt_server_write_attribute_value(
        gattdb_temperature_measurement, // handle from autogen/gatt_db.h
        0, // offset
        5, // length
        &htm_temperature_buffer[0]
  );
  if (sc != SL_STATUS_OK) {
      LOG_ERROR("Error setting GATT for HTM, Error Code: 0x%x\r\n", (uint16_t)sc);
  }

  // update notification to send
  notif_to_send.attribute = gattdb_temperature_measurement;
  notif_to_send.offset = 0;
  notif_to_send.value_len = 5;
  notif_to_send.value =  &htm_temperature_buffer[0]; // in IEEE-11073 format
  send_notification(&notif_to_send);

  // print temperature on lcd
  displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp = %d", temp_in_c);
  // update latest temperature value
  latest_temp = temp_in_c;
}
*/

void update_sound_level_gatt_and_send_notification(uint32_t mV){
  sl_status_t sc;
  size_t str_len;
  if (mV > 100){
      sound_ptr = "loud";
      str_len = 4;
  }
  else if (mV > 45){
      sound_ptr = "noisy";
      str_len = 5;
  }
  else{
      sound_ptr = "quiet";
      str_len = 5;
  }

  // write to gatt_db
  sc = sl_bt_gatt_server_write_attribute_value(
        gattdb_audio_input_description, // handle from autogen/gatt_db.h
        0, // offset
        str_len, // length
        (uint8_t*)sound_ptr
  );
  if (sc != SL_STATUS_OK) {
      LOG_ERROR("Error setting GATT for Sound Level, Error Code: 0x%x\r\n", (uint16_t)sc);
  }

  // update notification to send
  notif_to_send.attribute = gattdb_audio_input_description;
  notif_to_send.offset = 0;
  notif_to_send.value_len = str_len;
  notif_to_send.value = (uint8_t*)sound_ptr;
  send_notification(&notif_to_send);

  // update sound level on lcd
  displayPrintf(DISPLAY_ROW_SOUNDLEVEL, "Area is %s", sound_ptr);
}

void update_amb_light_gatt_and_send_notification(float lux){
  if (lux > 1888){ // max measurable from sensor
      amb_light_val = 1888;
  }
  else{
      amb_light_val = (uint16_t)lux;
  }
  sl_status_t sc;

  // write to gatt_db
  sc = sl_bt_gatt_server_write_attribute_value(
        gattdb_illuminance, // handle from autogen/gatt_db.h
        0, // offset
        2, // length
        (uint8_t*)amb_light_ptr
  );
  if (sc != SL_STATUS_OK) {
      LOG_ERROR("Error setting GATT for Ambient Light Level, Error Code: 0x%x\r\n", (uint16_t)sc);
  }

  // update notification to send
  notif_to_send.attribute = gattdb_illuminance;
  notif_to_send.offset = 0;
  notif_to_send.value_len = 2;
  notif_to_send.value = (uint8_t*)amb_light_ptr;
  send_notification(&notif_to_send);

  // update ambient light on lcd
  displayPrintf(DISPLAY_ROW_AMBLIGHTVALUE, "Light = %u lx", amb_light_val);
}

void update_space_occupied_gatt_and_send_notification(){
  sl_status_t sc;
  size_t str_len;
  if (space_occupied){
      occupied_ptr = "Occupied";
      str_len = 8;
  }
  else{
      occupied_ptr = "Available";
      str_len = 9;
  }

  // write to gatt_db
  sc = sl_bt_gatt_server_write_attribute_value(
        gattdb_space_occupied, // handle from autogen/gatt_db.h
        0, // offset
        str_len, // length
        (uint8_t*)occupied_ptr
  );
  if (sc != SL_STATUS_OK) {
      LOG_ERROR("Error setting GATT for Space Occupied, Error Code: 0x%x\r\n", (uint16_t)sc);
  }

  // update notification to send
  notif_to_send.attribute = gattdb_space_occupied;
  notif_to_send.offset = 0;
  notif_to_send.value_len = str_len;
  notif_to_send.value = (uint8_t*)occupied_ptr;
  send_notification(&notif_to_send);

  // update space occupied for LCD
  displayPrintf(DISPLAY_ROW_OCCUPIED, "%s", occupied_ptr);

  if (space_occupied){
      displayPrintf(DISPLAY_ROW_ACTION, "Press PB0 to");
      displayPrintf(DISPLAY_ROW_ACTION2, "mark available");
  }
  else{
      displayPrintf(DISPLAY_ROW_ACTION, "Press PB0 to");
      displayPrintf(DISPLAY_ROW_ACTION2, "mark occupied");
  }
}

#endif

ble_data_struct_t* get_ble_data(){
  return &ble_data;
}

// from lecture 10
void handle_ble_event(sl_bt_msg_t* evt){

  sl_status_t sc;
  sl_bt_evt_connection_opened_t bt_conn_open;
  //sl_bt_evt_connection_parameters_t bt_conn_param;
  uint32_t passkey;
  uint16_t characteristic;

#if DEVICE_IS_BLE_SERVER // for server states
  sl_bt_evt_gatt_server_characteristic_status_t gatt_server_char_status;
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
      // delete previous bonding
      sc = sl_bt_sm_delete_bondings();
      if (sc != SL_STATUS_OK){
          LOG_ERROR("Error deleting Bluetooth bondings, Error code: 0x%x\r\n", (uint16_t)sc);
      }
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
      displayPrintf(DISPLAY_ROW_NAME, "Study Space 1");
      displayPrintf(DISPLAY_ROW_BTADDR, "%02X:%02X:%02X:%02X:%02X:%02X",
                    ble_addr[0], ble_addr[1], ble_addr[2],
                    ble_addr[3], ble_addr[4], ble_addr[5]);
      displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
      displayPrintf(DISPLAY_ROW_TEAMNAME, "Team 13");

      if (space_occupied){
          displayPrintf(DISPLAY_ROW_OCCUPIED, "Occupied");
          displayPrintf(DISPLAY_ROW_ACTION, "Press PB0 to");
          displayPrintf(DISPLAY_ROW_ACTION2, "mark available");
      }
      else{
          displayPrintf(DISPLAY_ROW_OCCUPIED, "Available");
          displayPrintf(DISPLAY_ROW_ACTION, "Press PB0 to");
          displayPrintf(DISPLAY_ROW_ACTION2, "mark occupied");
      }

      // update security manager, enable bonding
      sl_bt_sm_configure(0x2F,   // bit 1 flag enables bonding
                         sl_bt_sm_io_capability_displayyesno);
      break;
    // Indicates that a new connection was opened
    case sl_bt_evt_connection_opened_id:
      bt_conn_open = evt->data.evt_connection_opened;

      // stop advertising once connection complete
      sc = sl_bt_advertiser_stop(ble_data.advertisingSetHandle);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error stopping Bluetooth advertising, Error code: 0x%x\r\n", (uint16_t)sc);
      }

      /*
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
      */

      ble_data.connectionHandle = bt_conn_open.connection;

      // display that server in connected mode
      displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
      break;
    // sl_bt_evt_connection_closed_id
    case sl_bt_evt_connection_closed_id:
      // update states
      ble_data.ok_to_send_htm_notifications = false;
      ble_data.ok_to_send_amb_light_notifications = false;
      ble_data.ok_to_send_sound_level_notifications= false;
      ble_data.ok_to_send_occupied_notifications = false;
      ble_data.passkey_received = false;
      ble_data.is_bonded = false;
      // turn LED off
      gpioLed0SetOff();

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
      displayPrintf(DISPLAY_ROW_PASSKEY, "");
      displayPrintf(DISPLAY_ROW_ACTION, "");
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
   // external events, only handles PB0 press here
    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & BLE_PB0_PRESS){
          // confirm BLE passkey if waiting
          if (ble_data.passkey_received){
              sc = sl_bt_sm_passkey_confirm(ble_data.connectionHandle, 1);
              if (sc != SL_STATUS_OK){
                 LOG_ERROR("Error confirming BLE passkey, Error code: 0x%x\r\n", (uint16_t)sc);
              }
              ble_data.passkey_received = false;
          }
          // update whether study space occupied or not
          else{
              space_occupied = !space_occupied;
              update_space_occupied_gatt_and_send_notification();
          }
      }
      else if (evt->data.evt_system_external_signal.extsignals & BLE_PB0_RELEASE){

      }
      break;
    // ******************************************************
    // Events for Server
    // ******************************************************
    // Indicates either: A local Client Characteristic Configuration descriptor (CCCD)
    // was changed by the remote GATT client, or
    // That a confirmation from the remote GATT Client was received upon a successful
    // reception of the indication I.e. we sent an indication from our server to the
    // client with sl_bt_gatt_server_notify_all()
    case sl_bt_evt_gatt_server_characteristic_status_id:
      // data received
      gatt_server_char_status = evt->data.evt_gatt_server_characteristic_status;

      // characteristic for CCCD change
      characteristic = gatt_server_char_status.characteristic;

      // CCCD changed
      if(gatt_server_char_status.status_flags & sl_bt_gatt_server_client_config){
          /*
          if (characteristic == gattdb_temperature_measurement){ // handle from gatt_db.h
              // notification flag
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_notification){
                   ble_data.ok_to_send_htm_notifications= true;
                   // print temperature on lcd
                   displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp = %d", latest_temp);
                   gpioLed0SetOn();
              }
              else{
                  ble_data.ok_to_send_htm_notifications = false;
                  // clear temperature on lcd
                  displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
                  gpioLed0SetOff();
              }
          }*/
          if (characteristic == gattdb_audio_input_description){
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_notification){
                  ble_data.ok_to_send_sound_level_notifications = true;
              }
              else{
                  ble_data.ok_to_send_sound_level_notifications = false;
              }
          }
          else if (characteristic == gattdb_illuminance){
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_notification){
                  ble_data.ok_to_send_amb_light_notifications = true;
              }
              else{
                  ble_data.ok_to_send_amb_light_notifications = false;
              }
          }
          else if (characteristic == gattdb_space_occupied){
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_notification){
                  ble_data.ok_to_send_occupied_notifications = true;
              }
              else{
                  ble_data.ok_to_send_occupied_notifications = false;
              }
          }
      }
      break;
    // Indicates confirmation from the remote GATT client has not been
    // received within 30 seconds after an indication was sent
    case sl_bt_evt_gatt_server_indication_timeout_id:
      LOG_ERROR("Received Indication Timeout\r\n");
      break;
    // Bluetooth soft timer interrupt (125 ms)
    case sl_bt_evt_sm_confirm_bonding_id:
      // accept bonding request
      sc = sl_bt_sm_bonding_confirm(ble_data.connectionHandle, 1);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error confirming BLE bonding, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    case sl_bt_evt_sm_confirm_passkey_id:
      passkey = evt->data.evt_sm_confirm_passkey.passkey;
      ble_data.passkey_received = true;
      // display passkey
      displayPrintf(DISPLAY_ROW_PASSKEY, "Passkey %06u", passkey);
      displayPrintf(DISPLAY_ROW_ACTION, "Confirm with PB0");

      // clear other action rows
      displayPrintf(DISPLAY_ROW_ACTION2, "");
      break;
    case sl_bt_evt_sm_bonded_id:
      displayPrintf(DISPLAY_ROW_CONNECTION, "Bonded");
      displayPrintf(DISPLAY_ROW_PASSKEY, "");
      displayPrintf(DISPLAY_ROW_ACTION, "");
      // reset passkey flags
      ble_data.is_bonded = true;

      // display space occupied action again
      if (space_occupied){
          displayPrintf(DISPLAY_ROW_OCCUPIED, "Occupied");
          displayPrintf(DISPLAY_ROW_ACTION, "Press PB0 to");
          displayPrintf(DISPLAY_ROW_ACTION2, "mark available");
      }
      else{
          displayPrintf(DISPLAY_ROW_OCCUPIED, "Available");
          displayPrintf(DISPLAY_ROW_ACTION, "Press PB0 to");
          displayPrintf(DISPLAY_ROW_ACTION2, "mark occupied");
      }
      break;
    case sl_bt_evt_sm_bonding_failed_id:
      LOG_ERROR("Bonding Failed\r\n");
      // close connection and reset
      sc = sl_bt_connection_close(ble_data.connectionHandle);
      if (sc != SL_STATUS_OK){
         LOG_ERROR("Error closing BLE connection, Error code: 0x%x\r\n", (uint16_t)sc);
      }
      break;
    default:
      break;
  }
#endif
}
