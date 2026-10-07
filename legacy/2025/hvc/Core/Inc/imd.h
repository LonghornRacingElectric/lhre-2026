//
// Created by Gautham Ramanarayanan on 2025/01/09.
//

#ifndef IMD_H
#define IMD_H

#include <stdbool.h>
#include <stdint.h>

#include "imd_mode_policy.h"

/* Bench-only bypass: PB1 never asserts the IMD latch, but the IMD logic still
   runs and is logged so the iso175's OK timing can be observed. */
#ifndef HVC_IMD_RELAY_HOLD_OVERRIDE
#define HVC_IMD_RELAY_HOLD_OVERRIDE 0
#endif

#define IMD_INFO_GENERAL 0x37
#define IMD_INFO_ISO_DETAIL 0x38
#define IMD_INFO_VOLTAGE 0x39
#define IMD_INFO_IT_SYS 0x3A
#define IMD_REQUEST 0x22
#define IMD_RESPONSE 0x23

float getImdOkVoltage(void);
bool isImdSignalOk(void);
bool isImdOk(void);
bool isImdBypassed(void);
bool isImdLatchSetAsserted(void);
imd_state_t getImdState(void);
imd_trip_reason_t getImdTripReason(void);
uint32_t getImdStateAgeMs(void);
void imd_can_init();
void imd_can_periodic(bool airsClosed);
void imd_send_cmd();

#endif //IMD_H
