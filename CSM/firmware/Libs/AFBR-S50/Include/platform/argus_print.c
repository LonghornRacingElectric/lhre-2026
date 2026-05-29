//
// Created by Gautham Ramanarayanan on 2/24/25. Modified by Arav Karnik on 4/1/2026
//

// #include "argus_print.h"

// #include <stdarg.h>
// #include <stdio.h>

// #include "usb_vcp.h"
// #include "stm32g4xx.h"

// status_t print(const char *fmt_s, ...)
// {
//     uint8_t sendMessage[256];
//     va_list args;
//     va_start(args, fmt_s);
//     vusb_printf(fmt_s, args);
//     va_end(args);
//     return SUCCESS;
// }


#include "argus_print.h"

#include <stdarg.h>
#include <stdio.h>

#include "usbd_cdc_if.h"

status_t print(const char *fmt_s, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt_s);
    int len = vsnprintf(buf, sizeof(buf), fmt_s, args);
    va_end(args);

    if (len > 0)
    {
        CDC_Transmit_FS((uint8_t *)buf, (uint16_t)len);
    }

    return STATUS_OK;
}