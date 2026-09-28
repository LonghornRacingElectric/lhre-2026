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
void contactors_init(void) {
    setTractiveContactor(false);
}

void setTractiveContactor(bool on) {
    HAL_GPIO_WritePin(CLOSE_IR_POS_GPIO_Port, CLOSE_IR_POS_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool isPosContactorClosed() {
    return HAL_GPIO_ReadPin(IR_POS_SENSE_GPIO_Port, IR_POS_SENSE_Pin) == GPIO_PIN_SET;
}

bool isNegContactorClosed() {
    return HAL_GPIO_ReadPin(IR_NEG_SENSE_GPIO_Port, IR_NEG_SENSE_Pin) == GPIO_PIN_SET;
}

bool isShutdownClosed(void) {
    #define SHUTDOWN_DEBOUNCE_COUNT 3U
    static bool debouncedState = false;
    static uint8_t debounceCounter = 0U;

    const bool currentReading = isNegContactorClosed();
    if (currentReading == debouncedState) {
        debounceCounter = 0U;
    } else if (++debounceCounter >= SHUTDOWN_DEBOUNCE_COUNT) {
        debouncedState = currentReading;
        debounceCounter = 0U;
    }

    return debouncedState;
}
