#include "rtos/usb.h"

#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "queue.h"
#include "semphr.h"
#include "stdbool.h"
#include "usb_base.h"

// wait 3(000) (m)s before failing
#define TIME_BEFORE_FAIL 3000

/**
 * Managing state for the USB system
 */
static SemaphoreHandle_t xPrintfMutex = NULL;
static bool initialized = false;

void init_usb(CDC_Transmit_Fn_ptr transmit_fn) {
    usb_init(transmit_fn);
    xPrintfMutex = xSemaphoreCreateMutex();
    initialized = true;

    if (!xPrintfMutex) {
        // error occurred, idk what to do here
        // for now, i'll just print to console that there's an error that
        // occurred
        usb_printf("Error occurred initializing the mutex.");
    }
}

void ts_printf(const char* pcFormat, ...) {
    if (!initialized) {
        return;
    }

    if (xPrintfMutex) {
        if (xSemaphoreTake(xPrintfMutex, pdMS_TO_TICKS(TIME_BEFORE_FAIL)) ==
            pdTRUE) {
            va_list args;
            va_start(args, pcFormat);
            v_usb_printf(pcFormat, args);
            va_end(args);
            xSemaphoreGive(xPrintfMutex);
        }
    } else {
        usb_printf(
            "Printing without thread safety. Data may be wrong. Check your "
            "code.");

        // there's no binary mutex, but we may as well send data
        // better than having no data at all
        va_list args;
        va_start(args, pcFormat);
        v_usb_printf(pcFormat, args);
        va_end(args);
    }
}
