/***********************************************************************
 * @file      ble.h
 * @brief     BLE module header for Blue Gecko
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Mar 7, 2025
 *
 * @resources Assignment 5 specs
 *
 *
 *
 */

#ifndef GECKO_BLE_H
#define GECKO_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_bluetooth.h"

#define UINT8_TO_BITSTREAM(p, n)  { *(p)++ = (uint8_t)(n); }

#define UINT32_TO_BITSTREAM(p, n) { *(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                    *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24); }

#define INT32_TO_FLOAT(m, e) ( (int32_t) (((uint32_t) m) & 0x00FFFFFFU) | (((uint32_t) e) << 24) )


#if DEVICE_IS_BLE_SERVER
// Define BLE bit-flags
#define NO_FLAG 0x0
#define BLE_LETIMER0_UF_FLAG 0x1
#define BLE_LETIMER0_COMP1_FLAG 0x2
#define BLE_I2C_TRANSFER_FLAG 0x4
#define BLE_PB0_FLAG 0x8
#endif

// BLE Data Structure, save all of our private BT data in here.
// Modern C (circa 2021 does it this way)
// typedef ble_data_struct_t is referred to as an anonymous struct definition
typedef struct {
  // values that are common to servers and clients
  bd_addr myAddress;
  uint8_t myAddressType;
  // The advertising set handle allocated from Bluetooth stack.
  uint8_t advertisingSetHandle;
  uint8_t connectionHandle;
  bool connection_alive;
  bool ok_to_send_htm_indications;

  // values unique for server
  bool indication_in_flight;
  bool passkey_received;
  bool passkey_confirmed;

  // values unique for client
  bool gatt_service_found;
  bool gatt_characteristic_found;
  uint32_t htmServiceHandle;
  uint16_t tempMeasHandle;

} ble_data_struct_t;

// ble functions
ble_data_struct_t* get_ble_data();




#if DEVICE_IS_BLE_SERVER
void update_temp_meas_gatt_and_send_indication(int temp_in_c); // update temperature gatt and send indicator
void update_PB0_gatt(uint8_t value); // update PB0 value in gatt
void waitForPB0Press();
#endif
// handles all ble events, different implementation for server and client
void handle_ble_event(sl_bt_msg_t* evt);

#endif
