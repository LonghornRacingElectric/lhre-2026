#ifndef __CAN_H__
#define __CAN_H__

#include <stdbool.h>
#include <stdint.h>

void hvc_can_init(void);

void hvc_set_contactor_status(int state, bool pos, bool neg);

void hvc_set_cell_temperatures(float *cell_temps);

/* Charger TX — built by hvc_charger.c, sent by the CAN RTOS. */
void hvc_set_charge_command(float target_v, float current_limit_a,
                            uint8_t flags, uint8_t seq);

/* Charger RX — atomically copies the latest mailbox snapshot (populated by
 * the CAN RTOS RX task) into the output params. Caller does freshness
 * detection via seq_echo. */
void hvc_get_charger_status(float *v_out, float *c_out, uint8_t *flags_out,
                            uint8_t *state_out, uint8_t *plug_out,
                            uint8_t *seq_echo_out);

#endif
