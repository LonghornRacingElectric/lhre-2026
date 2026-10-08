#include "balance_pwm.h"

void balance_pwm_pack(bool enable,
                      const bool commands[BALANCE_CELL_COUNT],
                      uint8_t pwm_a[BALANCE_PWM_REGISTER_BYTES],
                      uint8_t pwm_b[BALANCE_PWM_REGISTER_BYTES]) {
    for (uint32_t ic = 0U; ic < BALANCE_BMB_COUNT; ++ic) {
        for (uint32_t byte = 0U; byte < 6U; ++byte) {
            pwm_a[ic * 6U + byte] = 0U;
            pwm_b[ic * 6U + byte] = byte < 2U ? 0U : 0xFFU;
        }
        if (!enable) continue;
        const uint32_t board = BALANCE_BMB_COUNT - ic - 1U;
        for (uint32_t cell = 0U; cell < BALANCE_CELLS_PER_BMB; ++cell) {
            if (!commands[board * BALANCE_CELLS_PER_BMB + cell]) continue;
            uint8_t *bank = cell < 12U ? pwm_a : pwm_b;
            const uint32_t local_cell = cell < 12U ? cell : cell - 12U;
            bank[ic * 6U + local_cell / 2U] |=
                (uint8_t)(0x0FU << (4U * (local_cell % 2U)));
        }
    }
}
