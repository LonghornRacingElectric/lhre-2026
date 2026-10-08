//
// Created by Dhairya Gupta on 3/1/25.
//

#include "dfu.h"

#include "usb_vcp.h"

static uint16_t boot0pin;
static GPIO_TypeDef *boot0GPIO;

void dfu_init(GPIO_TypeDef *boot0, uint16_t bootPin) {
    boot0pin = bootPin;
    boot0GPIO = boot0;
}

void boot_to_dfu() {
    HAL_GPIO_WritePin(boot0GPIO, boot0pin, 1);

    HAL_Delay(100);

    println("Rebooting...\n");
    HAL_NVIC_SystemReset();
}