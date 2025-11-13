#ifndef LOGGER_H
#define LOGGER_H

#include "usb_base.h"

/**
 * @brief Initializes the logging system.
 *
 * This function creates the message queue and the dedicated logger task.
 * It must be called once before any call to ts_printf(), typically
 * during OS initialization.
 *
 * @param transmit_fn the USB serial transmit function pointer
 * @return 0 on success, -1 on failure.
 */
int init_logging(CDC_Transmit_Fn_ptr transmit_fn);

/**
 * @brief Thread-safe, non-blocking printf.
 *
 * Formats a string and sends it to the logger task via a message queue.
 * This function is designed to be very fast and will not block the calling
 * task, even if the USB peripheral is busy.
 *
 * If the log queue is full, the message will be dropped.
 *
 * @param format The standard printf format string.
 * @param ...    Variable arguments for the format string.
 */
void ts_printf(const char* format, ...);

#endif