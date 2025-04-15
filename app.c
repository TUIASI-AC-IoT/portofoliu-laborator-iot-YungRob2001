/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
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
// Connection handle for the current connection
static uint8_t connection_handle = 0xFF;
// Flag for button notifications
bool button_io_notification_enabled = false;
// Button state
uint8_t button_state = 0;

void GPIO_ODD_IRQHandler(void)
{
  // Stergere flag intrerupere
  uint32_t interruptMask = GPIO_IntGet();
  GPIO_IntClear(interruptMask);

  // Citire noua stare a butonului in variabila button_state
  button_state = !GPIO_PinInGet(gpioPortC, 7);

  // Update button state in GATT database
  sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO, 0, sizeof(button_state), &button_state);

  // Send notification if enabled
  if (button_io_notification_enabled) {
    sl_bt_gatt_server_notify_all(gattdb_BUTTON_IO, sizeof(button_state), &button_state);
  }
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////

  // Activare ramura clock periferic GPIO
  CMU_ClockEnable(cmuClock_GPIO, true);
  // Configurare GPIOA 04 ca iesire (LED)
  GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1);
  // Configurare GPIOC 07 ca intrare (buton)
  GPIO_PinModeSet(gpioPortC, 7, gpioModeInputPullFilter, 1);
  // Configurare intrerupere pentru buton pe ambele fronturi
  GPIO_IntConfig(gpioPortC, 7, true, true, true);
  // Activare intrerupere
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);

  // Initialize button state
  uint8_t val = 1;
  sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO, 0, sizeof(val), &val);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
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
  uint8_t recv_val;
  size_t recv_len;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Configure security manager (flags, IO capability)
      sc = sl_bt_sm_configure(0x0F, sl_bt_sm_io_capability_displayonly);
      app_assert_status(sc);

      // Set a passkey (for example: 123456)
      sc = sl_bt_sm_set_passkey(123456);
      app_assert_status(sc);

      // Enable bonding
      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                               sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                       sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      // Store connection handle
      connection_handle = evt->data.evt_connection_opened.connection;

      // Initiate security negotiation
      sc = sl_bt_sm_increase_security(connection_handle);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Reset connection handle
      connection_handle = 0xFF;

      // Reset notification flag when connection is closed
      button_io_notification_enabled = false;

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                               sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                       sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates passkey needs to be displayed
    case sl_bt_evt_sm_passkey_display_id:
      // Display the passkey to the user via terminal
      app_log("Please enter this passkey on your phone: %06ld\r\n",
             (long)evt->data.evt_sm_passkey_display.passkey);
      break;

    // -------------------------------
    // This event indicates bonding was successful
    case sl_bt_evt_sm_bonded_id:
      app_log("Bonding successful!\r\n");
      break;

    // -------------------------------
    // This event indicates bonding failed
    case sl_bt_evt_sm_bonding_failed_id:
      app_log("Bonding failed with error: 0x%04x\r\n",
             evt->data.evt_sm_bonding_failed.reason);
      break;

    // -------------------------------
    // Optional: check security level
    case sl_bt_evt_connection_parameters_id:
      // Check security level
      app_log("Connection security level: %d\r\n",
             evt->data.evt_connection_parameters.security_mode);
      break;

    // -------------------------------
    // This event indicates the status of a characteristic has changed
    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (gattdb_BUTTON_IO == evt->data.evt_gatt_server_characteristic_status.characteristic) {
        if (evt->data.evt_gatt_server_characteristic_status.client_config_flags & sl_bt_gatt_notification) {
          app_log("Notificare activata pentru caracteristica BUTTON\r\n");
          // Setare flag care va fi folosit in logica aplicatiei
          // pentru generarea de notificari
          button_io_notification_enabled = true;
        } else {
          app_log("Notificare dezactivata pentru caracteristica BUTTON\r\n");
          // Resetare flag
          button_io_notification_enabled = false;
        }
      }
      break;

    // -------------------------------
    // This event indicates a characteristic value was written by a client
    case sl_bt_evt_gatt_server_attribute_value_id:
      if (gattdb_LED_IO == evt->data.evt_gatt_server_attribute_value.attribute) {
        sl_bt_gatt_server_read_attribute_value(gattdb_LED_IO, 0, sizeof(recv_val), &recv_len, &recv_val);

        if (recv_val) {
          // Aprinde LED
          GPIO_PinOutSet(gpioPortA, 4);
        } else {
          // Stinge LED
          GPIO_PinOutClear(gpioPortA, 4);
        }

        app_log("LED= %d\r\n", recv_val);
      }
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
