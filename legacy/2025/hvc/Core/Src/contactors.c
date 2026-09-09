//
// Created by Kaitlyn Chang on 1/8/2025.
//

#include <stdbool.h>
#include "contactors.h"
#include "main.h"

/*
Positive Coil (IR+):
- Controlled by firmware, on when voltage > 90% or when time > 5 seconds
Negative Coil (IR-):
Always closed when shutdown has power (HV key on)
Always open when shutdown loses power (HV key off/shutdown tripped)
*/
void setTractiveContactor(bool on) {
    HAL_GPIO_WritePin(CLOSE_IR_POS_GPIO_Port, CLOSE_IR_POS_Pin, on);
}

bool isPosContactorClosed() {
    return HAL_GPIO_ReadPin(IR_POS_SENSE_GPIO_Port, IR_POS_SENSE_Pin) == GPIO_PIN_SET;
}

bool isNegContactorClosed() {
    return HAL_GPIO_ReadPin(IR_NEG_SENSE_GPIO_Port, IR_NEG_SENSE_Pin) == GPIO_PIN_SET;
}