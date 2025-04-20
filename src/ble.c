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


// struct for indications queue
typedef struct{
  uint16_t attribute;
  uint16_t offset;
  size_t value_len;
  uint8_t* value;
} ble_indications_struct_t;


// BLE private data
ble_data_struct_t ble_data = {.myAddress = {{0}}, .myAddressType = 0,
                              .advertisingSetHandle = 0, .connectionHandle = 0,
                              .connection_alive = false,
                              .ok_to_send_htm_indications = false,
                              .ok_to_send_PB0_indications = false,
                              .passkey_received = false,
                              .is_bonded = false,
                              .indication_in_flight = false,
                              .htmServiceHandle = 0,
                              .tempMeasHandle = 0,
                              .readReqInFlight = false,
                              .PB0IndReqInFlight = false};


// buffers for server
#if DEVICE_IS_BLE_SERVER
// buffer for sending values for server
uint8_t htm_temperature_buffer[5];
uint8_t *htm_temperature_ptr = &htm_temperature_buffer[0];
uint32_t htm_temperature_flt;
uint8_t flags = 0x00;

uint8_t PB0_pressed = 0;
uint8_t *PB0_pressed_ptr = &PB0_pressed;

// for indications queue
ble_indications_struct_t ind_to_send;

#define QUEUE_DEPTH 10
ble_indications_struct_t ind_queue[QUEUE_DEPTH] = {{0, 0, 0, 0}};
uint32_t rd_index = 0;
uint32_t wr_index = 0;

// to send indications via timer
uint8_t lazy_timer_count = 0;

// from Assignment 0.5
uint32_t nextIdx(uint32_t Idx) {
  uint32_t new_idx = (Idx + 1) % QUEUE_DEPTH;
  return new_idx;
}

void reset_queue() {
  rd_index = 0;
  wr_index = 0;
}

uint32_t get_queue_depth() {
  // no "circling buffer"
  if( rd_index < wr_index ){
    return wr_index - rd_index;
  }
  // buffer "circles" to index 0
  else if (rd_index > wr_index){
    return (QUEUE_DEPTH - rd_index) + wr_index;
  }
  // empty buffer
  else{
    return 0;
  }
}

bool queue_is_full(){
  if (nextIdx(wr_index) == rd_index){
      return true;
  }
  else{
      return false;
  }
}

bool queue_is_empty(){
  if (rd_index == wr_index){
      return true;
  }
  else{
      return false;
  }
}

bool add_indication_to_queue(ble_indications_struct_t *indication){
  if (queue_is_full()){
      return false;
  }
  ind_queue[wr_index].attribute = indication->attribute;
  ind_queue[wr_index].offset = indication->offset;
  ind_queue[wr_index].value = indication->value;
  ind_queue[wr_index].value_len = indication->value_len;
  wr_index = nextIdx(wr_index);
  return true;
}

// returns false if queue empty, writes indication to indication_ptr if true
// does not increment rd_index
bool get_indication_from_queue(ble_indications_struct_t* indication_ptr){
  if (queue_is_empty()){
      return false;
  }
  // copy indication info
  indication_ptr->attribute = ind_queue[rd_index].attribute;
  indication_ptr->offset = ind_queue[rd_index].offset;
  indication_ptr->value = ind_queue[rd_index].value;
  indication_ptr->value_len = ind_queue[rd_index].value_len;

  return true;
}
void remove_first_indication_from_queue(){
  if (!queue_is_empty()){
      rd_index = nextIdx(rd_index);
  }
}
#endif

int latest_temp = 0;

#if DEVICE_IS_BLE_SERVER
#define FROM_INPUT true
#define FROM_QUEUE false
/*
// sends indication, returns false if not able to, adds it to queue, if indication enabled
// indication: pointer to indication data
// from_input: send data from input, otherwise it is sent from queue
 * return value: true if indication sent, false if added to queue or no indication sent
 */
bool send_indication(ble_indications_struct_t *indication, bool from_input){
  sl_status_t sc;
  // send from queue
  if (from_input){
      // do not send indication if not set
      if (indication->attribute == gattdb_temperature_measurement &&
          !ble_data.ok_to_send_htm_indications){
          return false;
      }
      if (indication->attribute == gattdb_button_state &&
          !ble_data.ok_to_send_PB0_indications){
          return false;
      }
      // unable to send since indication full or queue exist, add to queue
      if (ble_data.indication_in_flight ||
          !queue_is_empty()){
          add_indication_to_queue(indication);
          return false;
      }
      // otherwise send indication
      else{
          ble_data.indication_in_flight = true;
          // send indication
          sc = sl_bt_gatt_server_send_indication(
              ble_data.connectionHandle,
              indication->attribute, // from autogen/gattdb.h
              indication->value_len,
              indication->value);
          if (sc != SL_STATUS_OK) {
              LOG_ERROR("Error sending GATT indication, Error Code: 0x%x\r\n", (uint16_t)sc);
          }
      }
  }
  // send indication from input
  else{
      bool success = get_indication_from_queue(&ind_to_send);
      // removed from queue
      if(success){
          // not ok to send
          if (ind_to_send.attribute == gattdb_temperature_measurement &&
              !ble_data.ok_to_send_htm_indications){
              return false;
          }
          if (ind_to_send.attribute == gattdb_button_state &&
              !ble_data.ok_to_send_PB0_indications){
              return false;
          }
          if (ble_data.indication_in_flight){
              return false;
          }

          remove_first_indication_from_queue();
          ble_data.indication_in_flight = true;
          // send indication
          sc = sl_bt_gatt_server_send_indication(
              ble_data.connectionHandle,
              ind_to_send.attribute,
              ind_to_send.value_len,
              ind_to_send.value);
          if (sc != SL_STATUS_OK) {
              LOG_ERROR("Error sending GATT indication, Error Code: 0x%x\r\n", (uint16_t)sc);
          }
      }
      // queue empty
      else{
          return false;
      }
  }
  return true;
}

// Referenced from Lecture 10 slides
void update_temp_meas_gatt_and_send_indication(int temp_in_c){
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

  // update indication to send
  ind_to_send.attribute = gattdb_temperature_measurement;
  ind_to_send.offset = 0;
  ind_to_send.value_len = 5;
  ind_to_send.value =  &htm_temperature_buffer[0]; // in IEEE-11073 format
  send_indication(&ind_to_send, FROM_INPUT);

   // print temperature on lcd
   displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", temp_in_c);
   // update latest temperature value
   latest_temp = temp_in_c;
}

void update_PB0_gatt_and_send_indication(uint8_t value){
  sl_status_t sc;
  PB0_pressed = value;

  // write to gatt_db
  sc = sl_bt_gatt_server_write_attribute_value(
        gattdb_button_state, // handle from autogen/gatt_db.h
        0, // offset
        1, // length
        PB0_pressed_ptr
  );
  if (sc != SL_STATUS_OK) {
      LOG_ERROR("Error setting GATT for PB0, Error Code: 0x%x\r\n", (uint16_t)sc);
  }

  // update indication to send
  ind_to_send.attribute = gattdb_button_state;
  ind_to_send.offset = 0;
  ind_to_send.value_len = 1;
  ind_to_send.value = PB0_pressed_ptr;
  send_indication(&ind_to_send, FROM_INPUT);
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
  unsigned int PB0_val;
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
      displayPrintf(DISPLAY_ROW_NAME, "Server");
      displayPrintf(DISPLAY_ROW_BTADDR, "%02X:%02X:%02X:%02X:%02X:%02X",
                    ble_addr[0], ble_addr[1], ble_addr[2],
                    ble_addr[3], ble_addr[4], ble_addr[5]);
      displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
      displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A8");

      // Display PB0 state
      PB0_val = gpioRead_PB0(); // 1 if released, 0 if pressed
      // low if pressed
      if (PB0_val){
          displayPrintf(DISPLAY_ROW_9, "Button Released");
      }
      else{
          displayPrintf(DISPLAY_ROW_9, "Button Pressed");
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
      ble_data.connection_alive = true;

      // display that server in connected mode
      displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
      break;
    // sl_bt_evt_connection_closed_id
    case sl_bt_evt_connection_closed_id:
      // update states
      ble_data.ok_to_send_htm_indications = false;
      ble_data.ok_to_send_PB0_indications = false;
      ble_data.indication_in_flight = false;
      ble_data.connection_alive = false;
      ble_data.passkey_received = false;
      ble_data.is_bonded = false;
      // turn LED off
      gpioLed0SetOff();
      gpioLed1SetOff();
      reset_queue();

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
          displayPrintf(DISPLAY_ROW_9, "Button Pressed");
          update_PB0_gatt_and_send_indication(1); // does not send indication if not enabled
          // confirm BLE passkey if waiting
          if (ble_data.passkey_received){
              sc = sl_bt_sm_passkey_confirm(ble_data.connectionHandle, 1);
              if (sc != SL_STATUS_OK){
                 LOG_ERROR("Error confirming BLE passkey, Error code: 0x%x\r\n", (uint16_t)sc);
              }
              ble_data.passkey_received = false;
          }
      }
      else if (evt->data.evt_system_external_signal.extsignals & BLE_PB0_RELEASE){
          displayPrintf(DISPLAY_ROW_9, "Button Released");
          update_PB0_gatt_and_send_indication(0); // does not send indication if not enabled
      }
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
                  if (ble_data.connection_alive){
                     ble_data.ok_to_send_htm_indications = true;
                     // print temperature on lcd
                     displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", latest_temp);
                     gpioLed0SetOn();
                  }
              }
              else{
                  if(ble_data.connection_alive){
                    ble_data.ok_to_send_htm_indications = false;
                    // clear temperature on lcd
                    displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
                    gpioLed0SetOff();
                  }
              }
              //LOG_INFO("HTM_INDICATION CHANGED! Value: %x\r\n", gatt_server_char_status.client_config_flags);
          }
          if (characteristic == gattdb_button_state){
              // indication flag
              if (gatt_server_char_status.client_config_flags & sl_bt_gatt_indication){
                  if(ble_data.is_bonded){
                     ble_data.ok_to_send_PB0_indications= true;
                     gpioLed1SetOn();
                  }
              }
              else{
                  if(ble_data.is_bonded){
                    ble_data.ok_to_send_PB0_indications = false;
                    gpioLed1SetOff();
                  }
              }
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
      LOG_ERROR("Received Indication Timeout\r\n");
      break;
    // Bluetooth soft timer interrupt (125 ms)
    case sl_bt_evt_system_soft_timer_id:
      // send_indication() does not send indication if queue empty
      send_indication(&ind_to_send, FROM_QUEUE);

      // refresh LCD every 8 * 125ms
      if (lazy_timer_count == 7){
        displayUpdate(); // prevent charge buildup within the Liquid Crystal Cells
        lazy_timer_count = 0;
      }
      else{
          lazy_timer_count++;
      }
      break;
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
      break;
    case sl_bt_evt_sm_bonded_id:
      displayPrintf(DISPLAY_ROW_CONNECTION, "Bonded");
      displayPrintf(DISPLAY_ROW_PASSKEY, "");
      displayPrintf(DISPLAY_ROW_ACTION, "");
      // reset passkey flags
      ble_data.is_bonded = true;
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
