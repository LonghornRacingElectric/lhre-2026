#ifndef IMD_MODE_POLICY_H
#define IMD_MODE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* The iso175 measures on the vehicle side of the AIRs, so it reports a fault
   whenever they are open. Its OK signal is therefore only judged once both
   AIRs are closed. */

/* IMD OK must be held this long after the AIRs close before faults count. */
#define IMD_OK_QUALIFY_MS 500U
/* iso175 response time is <= 30 s (R_iso <= 500 Ohm/V, Ce <= 2 uF): no OK
   within this long after the AIRs close is treated as a fault. */
#define IMD_OK_TIMEOUT_MS 30000U
/* A fault while armed must persist this long before it trips. */
#define IMD_FAULT_DEBOUNCE_MS 100U

typedef enum {
    IMD_STATE_DISARMED = 0, /* an AIR is open: IMD fault expected, ignored */
    IMD_STATE_WAIT_OK,      /* AIRs closed, waiting for the IMD to read OK */
    IMD_STATE_ARMED,        /* IMD read OK; a sustained fault trips */
    IMD_STATE_TRIPPED,      /* latch set until power cycle */
} imd_state_t;

typedef enum {
    IMD_TRIP_NONE = 0,
    IMD_TRIP_FAULT,   /* IMD OK dropped while armed */
    IMD_TRIP_TIMEOUT, /* IMD never read OK after the AIRs closed */
} imd_trip_reason_t;

typedef struct {
    imd_state_t state;
    imd_trip_reason_t tripReason;
    uint32_t stateSinceMs;
    bool okSeen;
    uint32_t okSinceMs;
    bool faultSeen;
    uint32_t faultSinceMs;
} imd_mode_policy_t;

void imd_mode_policy_init(imd_mode_policy_t *policy, uint32_t nowMs);

/* Call every loop. Returns the new state; the SR latch set output (PB1)
   should be high exactly when the state is IMD_STATE_TRIPPED. */
imd_state_t imd_mode_policy_update(imd_mode_policy_t *policy,
                                   bool airsClosed,
                                   bool imdOk,
                                   uint32_t nowMs);

bool imd_mode_policy_tripped(const imd_mode_policy_t *policy);
const char *imd_mode_policy_state_name(imd_state_t state);
const char *imd_mode_policy_trip_name(imd_trip_reason_t reason);

#endif // IMD_MODE_POLICY_H
