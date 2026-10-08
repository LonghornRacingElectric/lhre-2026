#ifndef BALANCE_PWM_H
#define BALANCE_PWM_H

#include <stdbool.h>
#include <stdint.h>

#include "balance_policy.h"

#define BALANCE_PWM_REGISTER_BYTES (BALANCE_BMB_COUNT * 6U)

void balance_pwm_pack(bool enable,
                      const bool commands[BALANCE_CELL_COUNT],
                      uint8_t pwm_a[BALANCE_PWM_REGISTER_BYTES],
                      uint8_t pwm_b[BALANCE_PWM_REGISTER_BYTES]);

#endif
