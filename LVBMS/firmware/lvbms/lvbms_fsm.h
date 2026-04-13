#ifndef LVBMS_FSM_H
#define LVBMS_FSM_H

#include <stdint.h>
#include "adBms6830Data.h"
#include "adBms6830GenericType.h"

/* ── Limits ─────────────────────────────────────────────────────── */
#define NUM_CELLS            7
#define BALANCE_THRESHOLD_V  0.010f   /* 10 mV */
#define BALANCE_PWM_DUTY     0x4      /* ~25% duty cycle */
#define OV_LIMIT_V           4.20f
#define UV_LIMIT_V           3.00f
#define OT_LIMIT_C           75.0f
#define OC_LIMIT_A           29.0f
#define BMS_TASK_PERIOD_MS   100

/* ── FSM States ──────────────────────────────────────────────────── */
typedef enum {
    BMS_STATE_INIT,
    BMS_STATE_IDLE,
    BMS_STATE_MEASURING,
    BMS_STATE_BALANCING,
    BMS_STATE_FAULT,
    BMS_STATE_SHUTDOWN
} BmsState_t;

/* ── Fault Flags ─────────────────────────────────────────────────── */
typedef enum {
    BMS_FAULT_NONE = 0,
    BMS_FAULT_OV   = (1 << 0),
    BMS_FAULT_UV   = (1 << 1),
    BMS_FAULT_OT   = (1 << 2),
    BMS_FAULT_OC   = (1 << 3),
} BmsFault_t;

/* ── Public API ──────────────────────────────────────────────────── */
void bms_task_init(cell_asic *ic_array, uint8_t num_ic);
void bms_fsm_run(void);

/* Getters so defaultTask can still log data if you want */
BmsState_t  bms_get_state(void);
uint32_t    bms_get_faults(void);
float       bms_get_cell_voltage(uint8_t cell_index);
float       bms_get_temperature(uint8_t temp_index);
float       bms_get_current(void);

#endif /* BMS_TASK_H */