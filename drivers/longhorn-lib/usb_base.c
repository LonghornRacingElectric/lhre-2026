#include "longhorn/usb_base.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define OUT_BUFFER_SIZE 256
static CDC_Transmit_Fn_ptr transmit_fn;

void usb_init(CDC_Transmit_Fn_ptr transmit_function) {
    transmit_fn = transmit_function;
}

/** Buffers holding the message(s) that will be sent over USB. */
static char out_buffer[OUT_BUFFER_SIZE];

void usb_println(const char* buffer) {
    if (!transmit_fn) {
        return;
    }

    size_t len = strlen(buffer);

    if (len > OUT_BUFFER_SIZE - 3) {
        len = OUT_BUFFER_SIZE - 3;
    }

    memcpy(out_buffer, buffer, len);

    out_buffer[len] = '\r';
    out_buffer[len + 1] = '\n';

    // Transmit the global buffer
    transmit_fn((uint8_t*)out_buffer, len + 2);
}

void v_usb_printf(const char* format, va_list args) {
    if (!transmit_fn) {
        return;
    }

    int len = vsnprintf(out_buffer, OUT_BUFFER_SIZE - 2, format, args);

    if (len < 0) {
        len = 0;
    }

    out_buffer[len] = '\r';
    out_buffer[len + 1] = '\n';

    // Transmit the global buffer
    transmit_fn((uint8_t*)out_buffer, len + 2);
}

void usb_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    v_usb_printf(format, args);
    va_end(args);
}
