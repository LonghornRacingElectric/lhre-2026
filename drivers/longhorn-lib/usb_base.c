#include "usb_base.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define OUT_BUFFER_SIZE 256
static CDC_Transmit_Fn_ptr transmit_fn;

void usb_init(CDC_Transmit_Fn_ptr transmit_function) {
    transmit_fn = transmit_function;
}

/** Buffer holding the message(s) that will be sent over USB. */
static char out_buffer[OUT_BUFFER_SIZE];

void usb_println(const char* buffer) {
    if (!transmit_fn) {
        return;
    }

    size_t len = strlen(buffer);

    // Truncate if the string is too long (to fit \r\n)
    if (len > OUT_BUFFER_SIZE - 3) {
        len = OUT_BUFFER_SIZE - 3;
    }

    char buf[len + 2];

    memcpy(buf, buffer, len);

    buf[len] = '\r';
    buf[len + 1] = '\n';

    // Transmit the global buffer. The flag will be cleared in the callback.
    transmit_fn((uint8_t*)buf, len + 2);
}

void usb_printf(const char* format, ...) {
    if (!transmit_fn) {
        return;
    }

    va_list args;
    va_start(args, format);

    int len = vsnprintf(out_buffer, OUT_BUFFER_SIZE - 2, format, args);
    va_end(args);

    if (len < 0) {
        len = 0;
    }

    out_buffer[len] = '\r';
    out_buffer[len + 1] = '\n';

    // Transmit the global buffer
    transmit_fn((uint8_t*)out_buffer, len + 2);
}
