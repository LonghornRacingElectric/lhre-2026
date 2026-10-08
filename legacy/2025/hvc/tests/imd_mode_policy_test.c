#include "imd_mode_policy.h"

#include <assert.h>

int main(void)
{
    imd_gpio_mode_t mode = IMD_GPIO_DRIVE_LOW;

    /* Startup and Park hold the active-low relay path asserted. */
    mode = imd_mode_policy_update(mode, false, 0U);
    assert(mode == IMD_GPIO_DRIVE_LOW);
    mode = imd_mode_policy_update(mode, true, 0U);
    assert(mode == IMD_GPIO_DRIVE_LOW);

    /* A fresh Drive state releases the pin to the real IMD signal. */
    mode = imd_mode_policy_update(mode, true, 1U);
    assert(mode == IMD_GPIO_INPUT);

    /* CAN loss in Drive cannot turn the override back on. */
    mode = imd_mode_policy_update(mode, false, 0U);
    assert(mode == IMD_GPIO_INPUT);

    /* A fresh Park state explicitly restores the low output. */
    mode = imd_mode_policy_update(mode, true, 0U);
    assert(mode == IMD_GPIO_DRIVE_LOW);

    /* Unknown non-Park states are handled conservatively as input mode. */
    mode = imd_mode_policy_update(mode, true, 2U);
    assert(mode == IMD_GPIO_INPUT);
    return 0;
}
