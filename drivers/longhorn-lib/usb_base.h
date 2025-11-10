#ifndef DRIVERS_LONGHORN_LIB_USB_BASE_H
#define DRIVERS_LONGHORN_LIB_USB_BASE_H

#include <stdarg.h>
#include <stdint.h>

typedef uint8_t (*CDC_Transmit_Fn_ptr)(uint8_t* Buf, uint16_t Len);

/**
 * @brief Initialize USB Transmission Library with HAL Transmit function
 * pointer. This MUST be used before using any of the other USB methods,
 * otherwise they will silently fail.
 *
 * @param transmit_function
 */
void usb_init(CDC_Transmit_Fn_ptr transmit_function);

/**
 * @brief
 *
 * This function takes a format string and a variable number of arguments,
 * formats them into a string, and sends the result over the USB serial
 * connection. The implementation should handle the formatting and the
 * low-level USB transmission.
 *
 * @param format The format string, following standard printf conventions.
 * @param ... The variable arguments to be formatted.
 */
void usb_printf(const char* format, ...);

/**
 * @brief
 *
 * This function is similar to usb_printf but takes a va_list argument
 * directly. It is useful for creating other variadic functions that
 * need to print formatted output.
 *
 * @param format The format string, following standard printf conventions.
 * @param args A va_list of the arguments to be formatted.
 */
void usb_vprintf(const char* format, va_list args);

#endif  // DRIVERS_LONGHORN_LIB_USB_BASE_H
