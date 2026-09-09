//
// Created by Dhairya Gupta on 1/19/25.
//

#include "usb_vcp.h"
#ifdef USB_VCP
#include <ctype.h>

#include "dfu.h"
#include "stdarg.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#define BUFFER_SIZE 16
#define OUT_BUFFER_SIZE 256

#define DRIVE_CMD_TEST "drive."
#define STOP_CMD_TEST "stop."

static int dfu_enable = 0;
volatile uint8_t receivedNotRead = 0;
uint8_t message[BUFFER_SIZE];
uint8_t idx = 0;
uint8_t sendMessage[OUT_BUFFER_SIZE];
static uint8_t txMessage[OUT_BUFFER_SIZE + 2];

int drive;

void println(char *buffer) {
    size_t len = strlen(buffer);
    if (len >= OUT_BUFFER_SIZE) {
        len = OUT_BUFFER_SIZE - 1;
    }
    memcpy(txMessage, buffer, len);
    txMessage[len] = '\r';
    txMessage[len + 1] = '\n';
#ifdef STM32L4
    CDC_Transmit_FS(txMessage, len + 2);
#elif defined(STM32H7) || defined(STM32H733xx)
    CDC_Transmit_HS(txMessage, len + 2);
#endif
}

void usb_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(sendMessage, OUT_BUFFER_SIZE, format, args);
    va_end(args);
    println(sendMessage);
}

void vusb_printf(const char *format, va_list args) {
    vsnprintf(sendMessage, OUT_BUFFER_SIZE, format, args);
    println(sendMessage);
}

void receiveData(uint8_t *data, uint32_t len) {
    idx = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == '\0' || data[i] == '\n' || data[i] == '\r') {
            // end of message
            message[idx] = '\0';
        } else {
            message[idx++] = tolower(data[i]);
        }

        if (idx >= BUFFER_SIZE) {
            idx = 0;
            // overflow
        }
    }

    if (!strcmp(message, DFU_COMMAND)) {
        dfu_enable = 1;
    }

    if (!strcmp(message, DRIVE_CMD_TEST)) {
        drive = 1;
    }

    if (!strcmp(message, STOP_CMD_TEST)) {
        drive = 0;
    }

    receivedNotRead = 1;
}

void receive_periodic() {
    if (receivedNotRead) {
        receivedNotRead = 0;
        if (dfu_enable) {
#ifdef SELF_BOOT_DFU
            println("Restarting in DFU mode...");
            boot_to_dfu();
#else
            println(
                "Device not configured to enter DFU mode... check the "
                "code.");
            dfu_enable = 0;
#endif
        }
    }
}

/**
 * Returns 1 if the DIGITAL drive switch is enabled, 0 otherwise
 * @return
 */
int checkDrive() { return drive; }

#endif
