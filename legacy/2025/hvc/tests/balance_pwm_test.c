#include "balance_pwm.h"

#include <assert.h>

int main(void) {
    bool commands[BALANCE_CELL_COUNT] = {false};
    uint8_t pwm_a[BALANCE_PWM_REGISTER_BYTES];
    uint8_t pwm_b[BALANCE_PWM_REGISTER_BYTES];

    commands[0] = true;    /* BMB01 C1 -> last IC, PWM A low nibble */
    commands[13] = true;   /* BMB01 C14 -> last IC, PWM B high nibble */
    commands[138] = true;  /* BMB10 C13 -> first IC, PWM B low nibble */
    balance_pwm_pack(true, commands, pwm_a, pwm_b);
    assert(pwm_a[9U * 6U] == 0x0FU);
    assert(pwm_b[9U * 6U] == 0xF0U);
    assert(pwm_b[0] == 0x0FU);
    assert(pwm_b[1] == 0U);
    assert(pwm_b[2] == 0xFFU);
    assert(pwm_a[0] == 0U);

    balance_pwm_pack(false, commands, pwm_a, pwm_b);
    for (uint32_t ic = 0U; ic < BALANCE_BMB_COUNT; ++ic) {
        for (uint32_t byte = 0U; byte < 6U; ++byte) {
            assert(pwm_a[ic * 6U + byte] == 0U);
            assert(pwm_b[ic * 6U + byte] == (byte < 2U ? 0U : 0xFFU));
        }
    }
    return 0;
}
