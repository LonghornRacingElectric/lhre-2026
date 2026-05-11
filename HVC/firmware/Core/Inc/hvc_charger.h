/**
 ******************************************************************************
 * @file    hvc_charger.h
 * @brief   HVC Charger Communication and CC/CV Algorithm
 * @details Computes the Charge Command sent to the charger board, consumes
 *          Charger Status from it, and exposes plug/state queries to the
 *          rest of HVC (state machine, faults).
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#ifndef HVC_CHARGER_H
#define HVC_CHARGER_H

#include <stdbool.h>
#include <stdint.h>

/* Charger states reported in Charger Status (matches CSV enum). */
typedef enum {
    CHARGER_STATE_IDLE        = 0,
    CHARGER_STATE_CONNECTING  = 1,
    CHARGER_STATE_CC          = 2,
    CHARGER_STATE_CV          = 3,
    CHARGER_STATE_COMPLETE    = 4,
    CHARGER_STATE_FAULT       = 5,
} charger_state_t;

/* Plug status reported in Charger Status (matches CSV enum). */
typedef enum {
    PLUG_STATUS_UNPLUGGED  = 0,
    PLUG_STATUS_PLUGGED    = 1,
    PLUG_STATUS_READY      = 2,
    PLUG_STATUS_FAULT      = 3,
} plug_status_t;

/* Charger flags bitfield (matches CSV bit definitions in 0x201). Bit 3 is
 * "battery missing / reverse polarity" per the ELCON protocol — a real fault,
 * not a transient. Bits 5..7 are unspecified by the charger. */
#define CHARGER_FLAG_HW_FAIL            (1u << 0)
#define CHARGER_FLAG_OVERTEMP           (1u << 1)
#define CHARGER_FLAG_INPUT_V_FAULT      (1u << 2)
#define CHARGER_FLAG_BATTERY_DISCONNECT (1u << 3)
#define CHARGER_FLAG_COMM_FAULT         (1u << 4)

/* Initialize internal state. Called once at startup. */
void charger_init(void);

/*
 * Periodic update — call from the state-machine task (10 Hz).
 * Recomputes target_v / current_limit / flags from BMS state and pushes
 * them out as Charge Command via can.c. Also evaluates the comms
 * watchdog on the most recent Charger Status.
 */
void charger_update(void);

/* Queries used by the state machine and faults layer. */
bool charger_is_plugged(void);     /* PLUG_STATUS_PLUGGED or READY */
bool charger_is_ready(void);       /* PLUG_STATUS_READY only */
bool charger_comms_ok(void);       /* status frame seen recently AND seq_echo matches */
bool charger_has_hw_fault(void);   /* any non-spare bit in charger_flags */

#endif /* HVC_CHARGER_H */
