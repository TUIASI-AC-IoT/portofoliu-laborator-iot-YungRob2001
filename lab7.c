/***************************************************************************//**
 * @file app.c
 * @brief Bluetooth LE GATT Server Application
 * @version 1.0.0
 * @date 2023
 *
 * Laboratory 7 - Bluetooth LE GATT Server
 * BGM220P Explorer Kit
 *
 * This application implements a GATT server with LED control and button 
 * monitoring functionality through custom characteristics.
 ******************************************************************************/

#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "gatt_db.h"

#include "em_cmu.h"
#include "em_gpio.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// Global variables for GATT server functionality
static bool button_io_notification_enabled = false;
static uint8_t button_state = 1; // Button is pulled up by default (not pressed)
static uint8_t previous_button_state = 1;

// Connection handle for notifications
static uint8_t connection_handle = 0xFF;

/**************************************************************************//**
 * GPIO Interrupt Handler for Button
 *****************************************************************************/
void GPIO_ODD_IRQHandler(void)
{
  // Clear interrupt flag
  uint32_t interruptMask = GPIO_IntGet();
  GPIO_IntClear(interruptMask);
  
  // Read current button state (active low)
  button_state = GPIO_PinInGet(gpioPortC, 7) ? 0 : 1; // Invert logic
  
  // Check if state changed to avoid duplicate notifications
  if (button_state != previous_button_state) {
    app_log("Button %s\n", button_state ? "pressed" : "released");
    
    // Update GATT database with new button state
    sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO, 
                                           0, 
                                           sizeof(button_state), 
                                           &button_state);
    
    // Send notification if enabled and client is connected
    if (button_io_notification_enabled && connection_handle != 0xFF) {
      sl_status_t sc = sl_bt_gatt_server_notify_all(gattdb_BUTTON_IO,
                                                   sizeof(button_state),
                                                   &button_state);
      if (sc == SL_STATUS_OK) {
        app_log("Button notification sent: %d\n", button_state);
      } else {
        app_log("Failed to send notification: 0x%04X\n", sc);
      }
    }
    
    previous_button_state = button_state;
  }
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  app_log("GATT Server Application Started\n");
  
  // Enable GPIO clock
  CMU_ClockEnable(cmuClock_GPIO, true);
  
  // Configure GPIO A04 as output (LED) - initially OFF
  GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1); // 1 = LED OFF (active low)
  
  // Configure GPIO C07 as input with pull-up (Button)
  GPIO_PinModeSet(gpioPortC, 7, gpioModeInputPullFilter, 1);
  
  // Configure interrupt for button on both edges
  GPIO_IntConfig(gpioPortC, 7, true, true, true);
  
  // Enable interrupts
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
  
  // Initialize button state
  button_state = GPIO_PinInGet(gpioPortC, 7) ? 0 : 1; // Invert logic
  previous_button_state = button_state;
  
  app_log("GPIO configuration completed\n");
  app_log("LED: GPIO A04 (active low)\n");
  app_log("Button: GPIO C07 (active low, pull-up enabled)\n");
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  // Non-blocking application logic can be implemented here
  // All main functionality is event-driven
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
    // This event indicates the device has started and the radio is ready.
    case sl_bt_evt_system_boot_id:
      app_log("Bluetooth stack initialized\n");
      
      // Create an advertising set
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      
      // Start advertising and enable connections
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      
      app_log("Started advertising as 'IoT-YourName'\n");
      app_log("Waiting for connection...\n");
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      connection_handle = evt->data.evt_connection_opened.connection;
      app_log("Connection opened (handle: %d)\n", connection_handle);
      app_log("Client can now access GATT services\n");
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      connection_handle = 0xFF;
      button_io_notification_enabled = false;
      app_log("Connection closed (reason: 0x%04X)\n", 
              evt->data.evt_connection_closed.reason);
      
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      
      app_log("Restarted advertising\n");
      break;

    // -------------------------------
    // This event indicates that a GATT characteristic was written by client
    case sl_bt_evt_gatt_server_attribute_value_id:
      {
        uint16_t attribute = evt->data.evt_gatt_server_attribute_value.attribute;
        
        if (attribute == gattdb_LED_IO) {
          uint8_t recv_val;
          size_t recv_len;
          
          // Read the new LED value from GATT database
          sc = sl_bt_gatt_server_read_attribute_value(gattdb_LED_IO,
                                                     0,
                                                     sizeof(recv_val),
                                                     &recv_len,
                                                     &recv_val);
          
          if (sc == SL_STATUS_OK && recv_len > 0) {
            if (recv_val) {
              // Turn LED ON (active low, so set pin to 0)
              GPIO_PinOutClear(gpioPortA, 4);
              app_log("LED turned ON (value: %d)\n", recv_val);
            } else {
              // Turn LED OFF (active low, so set pin to 1)
              GPIO_PinOutSet(gpioPortA, 4);
              app_log("LED turned OFF (value: %d)\n", recv_val);
            }
          } else {
            app_log("Failed to read LED characteristic value\n");
          }
        }
      }
      break;

    // -------------------------------
    // This event indicates that notification/indication status changed
    case sl_bt_evt_gatt_server_characteristic_status_id:
      {
        uint16_t characteristic = evt->data.evt_gatt_server_characteristic_status.characteristic;
        uint8_t status_flags = evt->data.evt_gatt_server_characteristic_status.status_flags;
        uint8_t client_config = evt->data.evt_gatt_server_characteristic_status.client_config_flags;
        
        if (characteristic == gattdb_BUTTON_IO) {
          // Check if this is a client configuration change
          if (status_flags == sl_bt_gatt_server_client_config) {
            if (client_config & sl_bt_gatt_notification) {
              button_io_notification_enabled = true;
              app_log("Button notifications ENABLED\n");
              
              // Send initial button state
              sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO, 
                                                     0, 
                                                     sizeof(button_state), 
                                                     &button_state);
              sl_bt_gatt_server_notify_all(gattdb_BUTTON_IO,
                                          sizeof(button_state),
                                          &button_state);
              app_log("Initial button state sent: %d\n", button_state);
            } else {
              button_io_notification_enabled = false;
              app_log("Button notifications DISABLED\n");
            }
          }
        }
      }
      break;

    // -------------------------------
    // This event indicates that a GATT characteristic was read by client
    case sl_bt_evt_gatt_server_user_read_request_id:
      {
        uint16_t characteristic = evt->data.evt_gatt_server_user_read_request.characteristic;
        
        if (characteristic == gattdb_BUTTON_IO) {
          // Update button state before read
          button_state = GPIO_PinInGet(gpioPortC, 7) ? 0 : 1;
          sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO, 
                                                 0, 
                                                 sizeof(button_state), 
                                                 &button_state);
          app_log("Button state read: %d\n", button_state);
        }
      }
      break;

    // -------------------------------
    // Default event handler
    default:
      break;
  }
}