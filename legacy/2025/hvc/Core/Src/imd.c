//
// Created by Gautham Ramanarayanan on 2025/01/09.
//

#include "imd.h"

#include "fdcan.h"
#include "main.h"

bool isImdOk() {
    return HAL_GPIO_ReadPin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin) == GPIO_PIN_SET;
}

// Only use if GPIO is set to output (used for testing only)
void testSetIMD(bool error) {
    HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin, error);
}

void imd_can_init() {
    /* The Orion integration uses FDCAN1. IMD fault state is read from its
       dedicated hardware pin; the unused legacy IMD diagnostic bus remains
       disabled so it cannot conflict with the Orion CAN ISR. */
}

void imd_can_periodic() {

}

void imd_can_config() {

}
