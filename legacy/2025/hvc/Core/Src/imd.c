//
// Created by Gautham Ramanarayanan on 2025/01/09.
//

#include "imd.h"

#include "adc.h"
#include "main.h"

/* Board rework:
   - iso175 OKHS -> resistor divider -> PA3: ~3 V = IMD OK, 0 V = fault.
     PA3 is TEMP_ADC1 (ADC1 INP15), already sampled by the DMA scan as
     getTempOne(), so the signal is read through the ADC.
   - PB1 (IMD_ERROR) is the only driver of the IMD SR-latch set input
     (1 = fault): always a push-pull output. */
#define IMD_OK_THRESHOLD_V 1.5f

static imd_mode_policy_t imdPolicy;

float getImdOkVoltage(void) {
    return getTempOne();
}

bool isImdSignalOk(void) {
    return getImdOkVoltage() > IMD_OK_THRESHOLD_V;
}

static void writeLatchSet(bool fault) {
    HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin,
                      fault ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool isImdBypassed(void) {
    return HVC_IMD_RELAY_HOLD_OVERRIDE != 0;
}

bool isImdOk(void) {
    return isImdBypassed() || !imd_mode_policy_tripped(&imdPolicy);
}

bool isImdLatchSetAsserted(void) {
    return HAL_GPIO_ReadPin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin) == GPIO_PIN_SET;
}

imd_state_t getImdState(void) {
    return imdPolicy.state;
}

imd_trip_reason_t getImdTripReason(void) {
    return imdPolicy.tripReason;
}

uint32_t getImdStateAgeMs(void) {
    return HAL_GetTick() - imdPolicy.stateSinceMs;
}

void imd_can_init() {
    /* MX_GPIO_Init already made PB1 an output preloaded low; reassert it.
       The unused legacy IMD CAN bus remains disabled. */
    imd_mode_policy_init(&imdPolicy, HAL_GetTick());
    writeLatchSet(false);
}

void imd_can_periodic(bool airsClosed) {
    (void)imd_mode_policy_update(&imdPolicy, airsClosed, isImdSignalOk(),
                                 HAL_GetTick());
    writeLatchSet(!isImdOk());
}

void imd_can_config() {

}
