//
// Created by Dhairya Gupta on 3/1/25.
//

#ifndef VCU_FIRMWARE_2025_DFU_H
#define VCU_FIRMWARE_2025_DFU_H

#include "gpio.h"

/* To enable: define SELF_BOOT_DFU in the main.h file */
void dfu_init(GPIO_TypeDef *boot0, uint16_t bootPin);

/* DANGEROUS!! This will pull up boot0, you'll have to upload code */
void boot_to_dfu();

#endif  // VCU_FIRMWARE_2025_DFU_H
