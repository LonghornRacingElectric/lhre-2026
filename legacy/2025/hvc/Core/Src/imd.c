//
// Created by Gautham Ramanarayanan on 2025/01/09.
//

#include "imd.h"

#include "fdcan.h"
#include "imd_mode_policy.h"
#include "main.h"

static imd_gpio_mode_t imdGpioMode = IMD_GPIO_INPUT;

static void configureImdPinAsInput(void) {
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = IMD_ERROR_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(IMD_ERROR_GPIO_Port, &gpio);
}

static void configureImdPinAsOutputLow(void) {
    GPIO_InitTypeDef gpio = {0};

    /* Preload the output latch low before enabling output mode, preventing a
       high pulse during the Park transition. */
    HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin, GPIO_PIN_RESET);
    gpio.Pin = IMD_ERROR_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(IMD_ERROR_GPIO_Port, &gpio);
}

bool isImdPinOk(void) {
    return HAL_GPIO_ReadPin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin) == GPIO_PIN_SET;
}

bool isImdOk(void) {
    return imdGpioMode == IMD_GPIO_DRIVE_LOW || isImdPinOk();
}

bool isImdOverrideActive(void) {
    return imdGpioMode == IMD_GPIO_DRIVE_LOW;
}

// Only use if GPIO is set to output (used for testing only)
void testSetIMD(bool error) {
    HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin, error);
}

void imd_can_init() {
    /* The Orion integration uses the dedicated hardware pin. Hold its
       active-low relay path while the VCU reports Park, then release PB1 to
       an input when the VCU reports Drive. The unused legacy IMD CAN bus
       remains disabled. */
#if HVC_IMD_RELAY_HOLD_OVERRIDE || HVC_IMD_PARK_HOLD_OVERRIDE
    imdGpioMode = IMD_GPIO_DRIVE_LOW;
    configureImdPinAsOutputLow();
#else
    imdGpioMode = IMD_GPIO_INPUT;
    configureImdPinAsInput();
#endif
}

void imd_can_periodic(bool vcuStateValid, uint8_t prndlState) {
#if HVC_IMD_RELAY_HOLD_OVERRIDE
    (void)vcuStateValid;
    (void)prndlState;
    /* Explicit bench configuration: keep driving low indefinitely. */
    imdGpioMode = IMD_GPIO_DRIVE_LOW;
    HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin, GPIO_PIN_RESET);
#elif HVC_IMD_PARK_HOLD_OVERRIDE
    const imd_gpio_mode_t desiredMode = imd_mode_policy_update(
        imdGpioMode, vcuStateValid, prndlState);
    if (desiredMode == imdGpioMode) {
        if (imdGpioMode == IMD_GPIO_DRIVE_LOW) {
            /* Maintain the active-low output while parked. */
            HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin,
                              GPIO_PIN_RESET);
        }
        return;
    }

    if (desiredMode == IMD_GPIO_DRIVE_LOW) {
        configureImdPinAsOutputLow();
    } else {
        /* INPUT + NOPULL is high impedance; this never drives the pin high. */
        configureImdPinAsInput();
    }
    imdGpioMode = desiredMode;
#else
    (void)vcuStateValid;
    (void)prndlState;
    if (imdGpioMode != IMD_GPIO_INPUT) {
        configureImdPinAsInput();
        imdGpioMode = IMD_GPIO_INPUT;
    }
#endif
}

void imd_can_config() {

}
