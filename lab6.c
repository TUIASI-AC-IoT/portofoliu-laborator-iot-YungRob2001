/***************************************************************************//**
 * @file app.c
 * @brief iBeacon Proximity Detection Application
 * @version 1.0.0
 * @date 2023
 *
 * Laboratory 6 - IoT Infrastructure (Part II)
 * Bluetooth LE Beacons - BGM220P Explorer Kit
 *
 * This application scans for iBeacon advertisements and detects proximity
 * based on RSSI values. When a valid iBeacon is detected within 1 meter,
 * it displays a greeting message.
 ******************************************************************************/

#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"

// iBeacon configuration constants
#define IBEACON_UUID_SIZE           16
#define IBEACON_COMPANY_ID          0x004C  // Apple Company ID (little endian)
#define IBEACON_TYPE                0x02    // Proximity beacon
#define IBEACON_LENGTH              0x15    // 21 bytes remaining
#define MANUFACTURER_DATA_TYPE      0xFF
#define IBEACON_AD_LENGTH           0x1A    // 26 bytes total

 
static const uint8_t target_uuid[IBEACON_UUID_SIZE] = {
  0xaa, 0xaa, 0xaa, 0xaa,
  0xbb, 0xbb,
  0xcc, 0xcc,
  0xdd, 0xdd,
  0xee, 0xee, 0xee, 0xee, 0xee, 0xee
};

/ 
static const char* get_student_name(uint16_t major, uint16_t minor) {
  if (major == 0) {  
    switch (minor) {
      case 1: return "Student 1 from CI  ";
      case 2: return "Student 2 from CI ";
      case 3: return "Student 3 from CI  ";
      default: return "Unknown CI Student";
    }
  } else if (major == 1) {  
    switch (minor) {
      case 1: return "Student 1 from SSC  ";
      case 2: return "Student 2 from SSC  ";
      case 3: return "Student 3 from SSC  ";
      default: return "Unknown SSC Student";
    }
  }
  return "Unknown Student";
}

 
static bool parse_ibeacon_data(const uint8_t *data, uint8_t data_len,
                               uint16_t *major, uint16_t *minor, int8_t *tx_power) {
  uint8_t i = 0;
  
  while (i < data_len) {
    uint8_t ad_length = data[i];
    uint8_t ad_type = data[i + 1];
    
    if (ad_type == MANUFACTURER_DATA_TYPE && ad_length == IBEACON_AD_LENGTH) {
      const uint8_t *ad_data = &data[i + 2];
      
       
      if (ad_data[0] != (IBEACON_COMPANY_ID & 0xFF) || 
          ad_data[1] != ((IBEACON_COMPANY_ID >> 8) & 0xFF)) {
        app_log("Company ID mismatch: 0x%02X%02X\n", ad_data[1], ad_data[0]);
        return false;
      }
      
      // Type
      if (ad_data[2] != IBEACON_TYPE) {
        app_log("iBeacon type mismatch: 0x%02X\n", ad_data[2]);
        return false;
      }
      
      // iBeacon Length
      if (ad_data[3] != IBEACON_LENGTH) {
        app_log("iBeacon length mismatch: 0x%02X\n", ad_data[3]);
        return false;
      }
      
      // Verify UUID (16 bytes starting at offset 4)
      if (memcmp(&ad_data[4], target_uuid, IBEACON_UUID_SIZE) != 0) {
        app_log("UUID mismatch\n");
        return false;
      }
      
      // Extract Major  
      *major = (ad_data[20] << 8) | ad_data[21];
      
      // Extract Minor  
      *minor = (ad_data[22] << 8) | ad_data[23];
      
      // Extract TX Power  
      *tx_power = (int8_t)ad_data[24];
      
      return true;
    }
    
    // Move to next AD structure
    i += ad_length + 1;
  }
  
  return false;
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void)
{
  app_log("iBeacon Proximity Detection Application Started\n");
  app_log("Waiting for Bluetooth stack initialization...\n");
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
  // Non-blocking application logic can be implemented here
  // For this application, all logic is event-driven
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  
  switch (SL_BT_MSG_ID(evt->header)) {
    
    // -------------------------------
    // This event indicates the device has started and the radio is ready
    case sl_bt_evt_system_boot_id:
      app_log("Bluetooth stack initialized\n");
      app_log("Starting passive scanning for iBeacon advertisements...\n");
      
       
      sc = sl_bt_scanner_start(160,    // 100ms scanning interval 
                               160,    // 100ms scanning window  
                               1,      // Passive scanning
                               0);     // Discover all devices
      
      if (sc == SL_STATUS_OK) {
        app_log("Scanning started successfully\n");
      } else {
        app_log("Failed to start scanning: 0x%04X\n", sc);
      }
      break;
    
    
    // This event is triggered when an advertisement packet is received
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      {
        uint16_t major, minor;
        int8_t tx_power;
        int8_t rssi = evt->data.evt_scanner_legacy_advertisement_report.rssi;
        uint8_t data_len = evt->data.evt_scanner_legacy_advertisement_report.data.len;
        const uint8_t *data = evt->data.evt_scanner_legacy_advertisement_report.data.data;
        
        // Parsing iBeacon data
        if (parse_ibeacon_data(data, data_len, &major, &minor, &tx_power)) {
          app_log("Valid iBeacon detected:\n");
          app_log("  Major: %d, Minor: %d\n", major, minor);
          app_log("  TX Power: %d dBm\n", tx_power);
          app_log("  RSSI: %d dBm\n", rssi);
          
          // Check proximity  
          // If current RSSI is greater than reference TX Power, device is closer than 1m
          if (rssi > tx_power) {
            const char* student_name = get_student_name(major, minor);
            app_log("\n*** PROXIMITY DETECTED ***\n");
            app_log("Salut, %s!\n", student_name);
            app_log("Distance: < 1 meter\n");
            app_log("***************************\n\n");
          } else {
            app_log("Device detected but distance > 1 meter\n\n");
          }
        }
      }
      break;
    
    
    default:
      break;
  }
}