// Thread safe functions for FreeRTOS to send data over USB VCP
// These functions will be considered critical sections, with each
// call requiring the thread to acquire a semaphore.

#ifndef DRIVERS_LONGHORN_LIB_RTOS_USB_H
#define DRIVERS_LONGHORN_LIB_RTOS_USB_H

#include "FreeRTOS.h"
#include "longhorn/usb_base.h"

/**
 * @brief Initializes a thread-safe variant of the USB logging interface.
 *
 * @param transmit_fn the HAL transmit function for USB Serial
 */
void init_usb(CDC_Transmit_Fn_ptr transmit_fn);

/**
 * @brief PREFER TO USE log_printf WITH THE LOGGER THREAD OVER THIS. FreeRTOS
 * Thread safe implementation of printf (uses a binary mutex to lock thread
 * access)
 *
 *
 * @param format the formatting for printing
 * @param ... any arguments that should be applied to the format
 */
void ts_printf(const char* format, ...);

#endif
