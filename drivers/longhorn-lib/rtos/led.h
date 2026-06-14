#ifndef DRIVERS_LONGHORN_LIB_RTOS_LED_H
#define DRIVERS_LONGHORN_LIB_RTOS_LED_H

#include "cmsis_os2.h"
#include "longhorn/led_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts a new thread that runs every 33ms for the LED to rainbow.
 *
 *
 * @return osThreadId_t ID of the newly created thread/task in FreeRTOS
 */
osThreadId_t led_start_thread();

void led_stop_thread();

#ifdef __cplusplus
}
#endif

#endif  // LONGHORN_LIBRARY_2025_LED_H
