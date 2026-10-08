#include "imd_mode_policy.h"

#define VCU_PRNDL_PARK 0U

imd_gpio_mode_t imd_mode_policy_update(imd_gpio_mode_t currentMode,
                                       bool vcuStateValid,
                                       uint8_t prndlState)
{
    /* A timeout must never change the electrical state. This keeps the pin
       low during a startup CAN gap, but leaves it safely as an input if CAN
       is lost after the car has entered Drive. */
    if (!vcuStateValid) return currentMode;

    return prndlState == VCU_PRNDL_PARK
        ? IMD_GPIO_DRIVE_LOW
        : IMD_GPIO_INPUT;
}
