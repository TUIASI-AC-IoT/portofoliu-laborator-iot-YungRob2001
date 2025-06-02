/***************************************************************************//**
 * @file
 * @brief Entry point for application.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "app.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  #include "sl_power_manager.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
  #include "sl_system_kernel.h"
#else
  #include "sl_system_process_action.h"
#endif

/**************************************************************************//**
 * @brief Main program entry point.
 *****************************************************************************/
int main(void)
{
  // Initialize system, device, and stacks
  sl_system_init();

  // Initialize application-specific components
  app_init();

  // Setup application runtime (e.g., RTOS tasks if present)
  app_init_runtime();

#if defined(SL_CATALOG_KERNEL_PRESENT)
  // Start the RTOS kernel (will begin running tasks)
  sl_system_kernel_start();

#else
  // Super loop (bare-metal fallback)
  while (1) {
    // Allow Silicon Labs components to perform background tasks
    sl_system_process_action();

    // Run user application loop logic
    app_process_action();

    #if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
      // Enter sleep mode if allowed
      sl_power_manager_sleep();
    #endif
  }
#endif
}
