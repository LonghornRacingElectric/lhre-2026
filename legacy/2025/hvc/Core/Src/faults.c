#include "faults.h"

#include <stdbool.h>

#include "cells.h"
#include "main.h"

#define TIMER_BMS_COMMS_S 1.0f
#define TIMER_BMS_OVERVOLTAGE_S 10.0f
#define TIMER_BMS_UNDERVOLTAGE_S 10.0f
#define TIMER_BMS_OVERTEMP_S 5.0f

static uint32_t latchedFaults;
static uint32_t lastFaults;
static uint32_t lastUpdateMs;
static bool initialized;
static float commsTimerS;
static float overvoltageTimerS;
static float undervoltageTimerS;
static float overtemperatureTimerS;

static bool faultAfterDelay(bool condition, float *timerS, float delayS,
                            float deltaTimeS) {
    if (!condition) {
        *timerS = 0.0f;
        return false;
    }

    *timerS += deltaTimeS;
    return *timerS >= delayS;
}

void faults_init(void) {
    latchedFaults = 0U;
    lastFaults = 0U;
    lastUpdateMs = HAL_GetTick();
    initialized = false;
    commsTimerS = 0.0f;
    overvoltageTimerS = 0.0f;
    undervoltageTimerS = 0.0f;
    overtemperatureTimerS = 0.0f;
}

uint32_t get_faults(void) {
    const uint32_t nowMs = HAL_GetTick();
    const float deltaTimeS = initialized
        ? (float)(nowMs - lastUpdateMs) / 1000.0f
        : 0.0f;
    lastUpdateMs = nowMs;
    initialized = true;

    uint32_t faults = 0U;
    if (faultAfterDelay(getNumResponsiveChips() != NUM_BMS_ICS,
                        &commsTimerS, TIMER_BMS_COMMS_S, deltaTimeS)) {
        faults |= FAULT_BMS_COMMS;
    }
    if (faultAfterDelay(hasCellOvervoltage(), &overvoltageTimerS,
                        TIMER_BMS_OVERVOLTAGE_S, deltaTimeS)) {
        faults |= FAULT_BMS_OVERVOLTAGE;
    }
    if (faultAfterDelay(hasCellUndervoltage(), &undervoltageTimerS,
                        TIMER_BMS_UNDERVOLTAGE_S, deltaTimeS)) {
        faults |= FAULT_BMS_UNDERVOLTAGE;
    }
    if (faultAfterDelay(hasCellOvertemperature(), &overtemperatureTimerS,
                        TIMER_BMS_OVERTEMP_S, deltaTimeS)) {
        faults |= FAULT_BMS_OVERTEMP;
    }
    lastFaults = faults;
    return faults;
}

uint32_t get_last_faults(void) {
    return lastFaults;
}

void latch_faults(uint32_t faults) {
    latchedFaults |= faults;
}

uint32_t get_latched_faults(void) {
    return latchedFaults;
}
