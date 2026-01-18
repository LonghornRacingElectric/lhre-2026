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
 * @brief Sends a message with newline and return characters appended. This is
 * sent over to the USB serial interface. Note that this method does NOT apply
 * any formatting or styling to the output, nor does it accept inputs for such.
 *
 * @param message the message to print to the USB interface
 */
void usb_println(const char* message);

/**
 * @brief Prints a formatted string to the USB serial connection.
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
 * @brief Takes in a VA list and completes USB logic.
 *
 * @param format
 * @param args
 */
void v_usb_printf(const char* format, va_list args);

#endif  // DRIVERS_LONGHORN_LIB_USB_BASE_H
