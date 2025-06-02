/***************************************************************************//**
 * @file
 * @brief Refactored Core Application Logic
 *******************************************************************************
 * License: See original file.
 ******************************************************************************/

#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "em_device.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "gatt_db.h"
#include "app_log.h"
#include "sl_sleeptimer.h"

// -----------------------------------------------------------------------------
// Constants & Globals
// -----------------------------------------------------------------------------

static sl_sleeptimer_timer_handle_t periodic_timer;
static const uint32_t timer_interval_ms = 1000;

static uint32_t passkey = 123456;
static uint8_t counter_value = 0;
static bool displaying_passkey = false;
static bool security_enabled = false;

static volatile uint8_t button_state = 0;
static volatile uint8_t prev_button_state = 0;

static uint8_t connection_handle = 0xFF;
static uint8_t advertising_set_handle = 0xFF;
static bool button_io_notification_enabled = false;

// -----------------------------------------------------------------------------
// Function Prototypes
// -----------------------------------------------------------------------------

static void setup_gpio(void);
static void setup_button_interrupt(void);
static void start_periodic_timer(void);
static void display_passkey(uint32_t key);
static void display_connection_status(uint8_t status);
static void periodic_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

SL_WEAK void app_init(void)
{
  setup_gpio();
  setup_button_interrupt();
  start_periodic_timer();

  button_state = !GPIO_PinInGet(gpioPortC, 7);
  prev_button_state = button_state;
}

// -----------------------------------------------------------------------------
// Main Processing Loop
// -----------------------------------------------------------------------------

SL_WEAK void app_process_action(void)
{
  uint8_t current_state = !GPIO_PinInGet(gpioPortC, 7);
  if (current_state != prev_button_state) {
    button_state = current_state;
    prev_button_state = current_state;
    sl_bt_external_signal(1);  // Notify stack of button event
  }
}

// -----------------------------------------------------------------------------
// Bluetooth Stack Event Handler
// -----------------------------------------------------------------------------

void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  uint8_t recv_val;
  size_t recv_len;

  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      app_log("initializing  \r\n");

      sl_bt_sm_configure(0, sl_bt_sm_io_capability_displayonly);
      sl_bt_sm_set_passkey(passkey);
      sl_bt_sm_set_bondable_mode(1);

      sl_bt_advertiser_create_set(&advertising_set_handle);
      sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);

      sl_bt_advertiser_set_timing(advertising_set_handle, 160, 160, 0, 0);
      sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      break;

    case sl_bt_evt_connection_opened_id:
      connection_handle = evt->data.evt_connection_opened.connection;
      app_log("Connection opened\r\n");
      sl_bt_sm_increase_security(connection_handle);
      break;

    case sl_bt_evt_sm_passkey_display_id:
      displaying_passkey = true;
      passkey = evt->data.evt_sm_passkey_display.passkey;
      display_passkey(passkey);
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log("Bonding successful\r\n");
      security_enabled = true;
      displaying_passkey = false;
      display_connection_status(1);
      break;

    case sl_bt_evt_sm_bonding_failed_id:
      app_log("Bonding failed (0x%02X)\r\n", evt->data.evt_sm_bonding_failed.reason);
      security_enabled = false;
      displaying_passkey = false;
      display_connection_status(0);
      break;

    case sl_bt_evt_connection_closed_id:
      connection_handle = 0xFF;
      button_io_notification_enabled = false;

      sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
      sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_BUTTON_IO) {
        button_io_notification_enabled = 
          evt->data.evt_gatt_server_characteristic_status.client_config_flags & sl_bt_gatt_notification;
        app_log("Button notifications %s\r\n", button_io_notification_enabled ? "enabled" : "disabled");
      }
      break;

    case sl_bt_evt_gatt_server_attribute_value_id:
      if (evt->data.evt_gatt_server_attribute_value.attribute == gattdb_LED_IO) {
        sc = sl_bt_gatt_server_read_attribute_value(gattdb_LED_IO, 0, sizeof(recv_val), &recv_len, &recv_val);
        app_assert_status(sc);

        recv_val ? GPIO_PinOutSet(gpioPortA, 4) : GPIO_PinOutClear(gpioPortA, 4);
        app_log("LED = %d\r\n", recv_val);
      }
      break;

    case sl_bt_evt_system_external_signal_id:
      if (connection_handle != 0xFF) {
        uint8_t non_volatile_state = button_state;
        sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO, 0, sizeof(non_volatile_state), &non_volatile_state);

        if (button_io_notification_enabled) {
          sl_bt_gatt_server_notify_all(gattdb_BUTTON_IO, sizeof(non_volatile_state), &non_volatile_state);
        }
      }
      break;

    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// GPIO and Timer Setup
// -----------------------------------------------------------------------------

static void setup_gpio(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);

  GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1); // LED output
  GPIO_PinModeSet(gpioPortC, 7, gpioModeInputPullFilter, 1); // Button input
}

static void setup_button_interrupt(void)
{
  GPIO_ExtIntConfig(gpioPortC, 7, 7, true, true, true);
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
}

static void start_periodic_timer(void)
{
  sl_status_t sc = sl_sleeptimer_start_periodic_timer(&periodic_timer,
                                                      timer_interval_ms,
                                                      periodic_timer_callback,
                                                      NULL, 0, 0);
  app_assert_status(sc);
  app_log("Periodic timer started (%lu ms)\r\n", timer_interval_ms);
}

// -----------------------------------------------------------------------------
// Display Utilities
// -----------------------------------------------------------------------------

static void display_passkey(uint32_t key)
{
  app_log("PASSKEY: %06lu\r\n", key);

  for (int i = 0; i < 3; i++) {
    GPIO_PinOutToggle(gpioPortA, 4);
    sl_sleeptimer_delay_millisecond(300);
    GPIO_PinOutToggle(gpioPortA, 4);
    sl_sleeptimer_delay_millisecond(300);
  }
}

static void display_connection_status(uint8_t status)
{
  const int flash_count = status ? 5 : 3;
  const int delay = status ? 100 : 500;

  app_log(status ? "PAIRING SUCCESSFUL\r\n" : "PAIRING FAILED\r\n");

  for (int i = 0; i < flash_count; i++) {
    GPIO_PinOutToggle(gpioPortA, 4);
    sl_sleeptimer_delay_millisecond(delay);
    GPIO_PinOutToggle(gpioPortA, 4);
    sl_sleeptimer_delay_millisecond(delay);
  }
}

// -----------------------------------------------------------------------------
// Timer Callback
// -----------------------------------------------------------------------------

static void periodic_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  counter_value++;
  if (connection_handle != 0xFF) {
    app_log("Counter = %d\r\n", counter_value);
  }
}
