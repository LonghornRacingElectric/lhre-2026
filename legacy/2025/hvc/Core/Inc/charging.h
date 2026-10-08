#ifndef HVC_CHARGING_H
#define HVC_CHARGING_H

#include <stdbool.h>

#include "charge_policy.h"

typedef struct {
    charge_phase_t phase;
    charge_stop_reason_t stopReason;
    /* Last HVC charger command (0x050). */
    bool commandEnable;
    float commandVoltage;
    float commandCurrent;
    /* Last charger status (0x051). */
    bool chargerConnected;
    float chargerVoltage;
    float chargerCurrent;
    bool chargerEnabled;
} hvc_charging_status_t;

/* Runs CC/CV every state-machine tick and stages the 0x050 command.
   enable = the HVC is in the CHARGING state (AIRs closed). */
void hvc_control_charging(bool enable);
void hvc_get_charging_status(hvc_charging_status_t *status);

#endif // HVC_CHARGING_H
