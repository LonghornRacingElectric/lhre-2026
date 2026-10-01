#ifndef IMD_MODE_POLICY_H
#define IMD_MODE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IMD_GPIO_DRIVE_LOW = 0,
    IMD_GPIO_INPUT = 1,
} imd_gpio_mode_t;

imd_gpio_mode_t imd_mode_policy_update(imd_gpio_mode_t currentMode,
                                       bool vcuStateValid,
                                       uint8_t prndlState);

#endif // IMD_MODE_POLICY_H
