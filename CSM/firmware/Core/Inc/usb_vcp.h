#ifndef USB_VCP_H
#define USB_VCP_H

#include <stdint.h>
#include <stdarg.h>

// Stub implementation - USB VCP not available
// Replace with actual USB serial implementation later

static inline void usb_printf(const char* fmt, ...) {
    (void)fmt; // suppress unused warning
}

static inline void usb_putchar(uint8_t c) {
    (void)c;
}

#endif // USB_VCP_H