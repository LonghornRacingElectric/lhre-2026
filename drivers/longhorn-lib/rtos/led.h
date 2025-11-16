#ifndef DRIVERS_LONGHORN_LIB_RTOS_LED_H
#define DRIVERS_LONGHORN_LIB_RTOS_LED_H

#include "cmsis_os2.h"
#include "led_base.h"

/**
 * @brief Starts a new thread that runs every 33ms for the LED to rainbow.
 *
 *
 * @return osThreadId_t ID of the newly created thread/task in FreeRTOS
 */
osThreadId_t led_start_thread();

#endif  // LONGHORN_LIBRARY_2025_LED_H
