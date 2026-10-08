#ifndef CHARGE_POLICY_H
#define CHARGE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* CC/CV charging, ported from Orion's HVC (charger-algo branch,
   HVC/firmware/Core/Src/hvc_charger.c) onto the 0x050/0x051 charger packets.

   The charger regulates the pack to the commanded voltage (CV) and limits
   current to the commanded current (CC). The HVC tapers that current on the
   highest cell, so no single cell is pushed past CHARGE_CELL_TARGET_V while
   the rest of the pack catches up. */

#define CHARGE_CELL_TARGET_V        4.18f /* per-cell CV target */
#define CHARGE_CELL_TAPER_START_V   4.08f /* taper current from here up to target */
#define CHARGE_CELL_HARD_OV_V       4.20f /* any cell above: stop the charger */
/* Pack voltage the charger holds in CV. 137 cells at 4.18 V would be 572.7 V,
   but with ~150 mV of cell spread that lets the highest cells pass the
   target, so the pack is held at 560 V (4.09 V average). */
#define CHARGE_PACK_TARGET_V        560.0f
#define CHARGE_MAX_CURRENT_A        10.0f /* charger / pack current limit */
#define CHARGE_TERMINATION_A        0.5f  /* below this at target: charge done */
#define CHARGE_CUTOFF_TEMP_C        55.0f /* hottest cell at or above: stop */

typedef enum {
    CHARGE_PHASE_IDLE = 0, /* not in the HVC charging state */
    CHARGE_PHASE_CC,       /* full current, highest cell below taper start */
    CHARGE_PHASE_CV,       /* current tapering on the highest cell */
    CHARGE_PHASE_DONE,     /* terminated; stays until the charger is unplugged */
    CHARGE_PHASE_STOPPED,  /* hard stop, see stopReason */
} charge_phase_t;

typedef enum {
    CHARGE_STOP_NONE = 0,
    CHARGE_STOP_NO_CELL_DATA, /* BMBs not all read, or no valid max cell */
    CHARGE_STOP_BMS_FAULT,    /* latched BMS fault */
    CHARGE_STOP_IMD,          /* IMD tripped */
    CHARGE_STOP_CELL_OV,      /* a cell above CHARGE_CELL_HARD_OV_V */
    CHARGE_STOP_OVERTEMP,     /* a cell at or above CHARGE_CUTOFF_TEMP_C */
} charge_stop_reason_t;

typedef struct {
    bool chargingState;     /* HVC state machine is in CHARGING (AIRs closed) */
    bool chargerConnected;  /* charger status (0x051) received recently */
    bool cellDataValid;     /* every BMB read in the last scan */
    float maxCellV;
    float maxTempC;
    bool bmsFault;
    bool imdOk;
    float chargerCurrentA;  /* measured by the charger, from 0x051 */
} charge_inputs_t;

typedef struct {
    bool enable;
    float currentLimitA;
    charge_phase_t phase;
    charge_stop_reason_t stopReason;
} charge_outputs_t;

typedef struct {
    bool chargeDone;
} charge_policy_t;

void charge_policy_init(charge_policy_t *policy);

/* Call every state-machine tick (100 ms). The pack voltage target sent to the
   charger is constant: pack series cells x CHARGE_CELL_TARGET_V. */
charge_outputs_t charge_policy_update(charge_policy_t *policy,
                                      const charge_inputs_t *inputs);

const char *charge_policy_phase_name(charge_phase_t phase);
const char *charge_policy_stop_name(charge_stop_reason_t reason);

#endif // CHARGE_POLICY_H
