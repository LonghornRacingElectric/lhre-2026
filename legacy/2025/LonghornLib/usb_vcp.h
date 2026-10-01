//
// Created by Dhairya Gupta on 1/19/25.
//
#include <stdarg.h>
#include <stdint.h>

#include "main.h"
#ifndef VCU_FIRMWARE_2025_USB_VCP_H
#define VCU_FIRMWARE_2025_USB_VCP_H
#ifdef USB_VCP

void usb_init();

void println(char *buffer);
void usb_printf(const char *format, ...);
void vusb_printf(const char *format, va_list args);
void receiveData(uint8_t *data, uint32_t len);
void receive_periodic();
extern volatile uint8_t receivedNotRead;
int checkDrive();

#define DFU_COMMAND "update"

#endif
#endif  // VCU_FIRMWARE_2025_USB_VCP_H
