/**
 ******************************************************************************
 * @file    hvc_charger.c
 * @brief   HVC Charger Communication and CC/CV Algorithm
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#include "hvc_charger.h"

#include "can.h"
#include "cmsis_os2.h"
#include "hvc_bms.h"
#include "hvc_state_machine.h"
#include "hvc_states.h"
#include "longhorn/rtos/logger.h"

/* ==========================================================================
 * Tunables — review before deploying to real hardware.
 * ========================================================================== */

/* NUM_CELLS is the cell *voltage* count (one tap per series cell). The cell
 * temperature count is independent (90 thermistors across 23 CAN packets in
 * can.c) — not all cells are individually instrumented for temperature. */
#define NUM_CELLS                130
#define CELL_TARGET_V            4.18f   /* per-cell CV target */
#define CELL_TAPER_START_V       4.08f   /* start CV ramp this far below target */
#define CELL_HARD_OV_V           4.20f   /* hard OV: hard_stop the charger */

#define PACK_TARGET_V            (NUM_CELLS * CELL_TARGET_V)  /* 543.4 V */

#define CHARGER_MAX_A            10.0f   /* hardware limit of the charger */
#define TERMINATION_CURRENT_A    0.5f    /* below this in CV, charge is "done" */

#define CUTOFF_TEMP_C            55.0f   /* hard thermal cutoff */

/* Charge Command flag bits (must match CSV). */
#define CHG_FLAG_ENABLE          (1u << 0)
#define CHG_FLAG_HARD_STOP       (1u << 1)

/* Comms watchdog. seq_echo should track tx_seq within ~1 cycle when healthy;
 * we tolerate a small slop to absorb scheduling jitter. */
#define MAX_SEQ_LAG              3

/* ==========================================================================
 * Internal state. Single-writer (charger_update on the state-machine task).
 * ========================================================================== */

static uint8_t  tx_seq = 0;
static bool     charge_done = false;

/* Latest snapshot from the Charger Status mailbox; refreshed at the top of
 * each charger_update() so all queries within the same tick see a consistent
 * view. */
static float            cur_v       = 0.0f;
static float            cur_c       = 0.0f;
static uint8_t          cur_flags   = 0;
static charger_state_t  cur_state   = CHARGER_STATE_IDLE;
static plug_status_t    cur_plug    = PLUG_STATUS_UNPLUGGED;
static uint8_t          cur_seq_echo = 0;

/* ==========================================================================
 * Public init / queries.
 * ========================================================================== */

void charger_init(void)
{
    /* Start tx_seq high so charger_comms_ok() stays false until a real echo
     * arrives — without this, BSS-initialized 0 == 0 would falsely report
     * "comms ok" for one tick after boot. */
    tx_seq = MAX_SEQ_LAG + 1;
}

bool charger_is_plugged(void) {
    return (cur_plug == PLUG_STATUS_PLUGGED) || (cur_plug == PLUG_STATUS_READY);
}

bool charger_is_ready(void) {
    return cur_plug == PLUG_STATUS_READY;
}

bool charger_comms_ok(void) {
    uint8_t lag = (uint8_t)(tx_seq - cur_seq_echo);
    return lag <= MAX_SEQ_LAG;
}

bool charger_has_hw_fault(void) {
    /* Every charger status bit is a genuine fault (incl. bit 3 = battery
     * missing / reverse polarity). Unspecified bits are treated as faults too. */
    return cur_flags != 0;
}

/* ==========================================================================
 * CC/CV algorithm.
 * ========================================================================== */

/* Linear taper: returns 1.0 below taper_start, ramps to 0.0 at target. */
static float cv_taper_fraction(float v, float taper_start, float target)
{
    if (v <= taper_start) return 1.0f;
    if (v >= target)      return 0.0f;
    return (target - v) / (target - taper_start);
}

static float compute_current_limit_a(void)
{
    float max_cell = bms_get_max_voltage();
    float max_temp = bms_get_max_temp();

    /* Thermal cutoff overrides everything. */
    if (max_temp >= CUTOFF_TEMP_C) return 0.0f;
    float limit_a = CHARGER_MAX_A * cv_taper_fraction(max_cell, CELL_TAPER_START_V, CELL_TARGET_V);
    if (limit_a < 0.0f) limit_a = 0.0f;
    return limit_a;
}

void charger_update(void)
{
    /* Refresh the local snapshot from the CAN RX mailbox. Atomic per call. */
    uint8_t state_u8, plug_u8;
    hvc_get_charger_status(&cur_v, &cur_c, &cur_flags,
                           &state_u8, &plug_u8, &cur_seq_echo);
    cur_state = (charger_state_t)state_u8;
    cur_plug  = (plug_status_t)plug_u8;

    hvc_state_t hvc_state = get_current_state();
    /* Only command current once the main contactor is actually closed
     * (HVC_STATE_CHARGING). During CHARGING_PRECHARGING the tractive bus is
     * being brought up through the precharge resistor by the hardware
     * shutdown loop; the charger output must stay off until that completes. */
    bool charging_active = (hvc_state == HVC_STATE_CHARGING);

    if (!charger_is_plugged()) {
        charge_done = false;
    }

    bool hard_stop = bms_check_disconnection() ||
                     bms_check_overtemp() ||
                     (bms_get_max_voltage() > CELL_HARD_OV_V) ||
                     !is_shutdown_closed() ||
                     charger_has_hw_fault();

    /* Build the Charge Command. */
    float target_v_v      = PACK_TARGET_V;
    float current_limit_a = 0.0f;
    uint8_t flags         = 0;

    if (charging_active && !hard_stop && charger_is_ready() && charger_comms_ok() && !charge_done) {
        current_limit_a = compute_current_limit_a();
        flags |= CHG_FLAG_ENABLE;
        bool cv_complete = (bms_get_max_voltage() >= CELL_TARGET_V) &&
                           (cur_c < TERMINATION_CURRENT_A) &&
                           (cur_state == CHARGER_STATE_CC || cur_state == CHARGER_STATE_CV);
        if (cv_complete) {
            charge_done = true;
            flags &= ~CHG_FLAG_ENABLE;
            current_limit_a = 0.0f;
        }
    }

    if (hard_stop) {
        flags |= CHG_FLAG_HARD_STOP;
        flags &= ~CHG_FLAG_ENABLE;
        current_limit_a = 0.0f;
    }

    tx_seq++;  /* monotonic, wraps at 256 */

    hvc_set_charge_command(target_v_v, current_limit_a, flags, tx_seq);
}
