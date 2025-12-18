/**
 ******************************************************************************
 * @file    hvc_contactors.c
 * @brief   HVC Contactor Control Implementation
 * @details Controls high voltage contactors (precharge, AIR+, AIR-)
 *          Based on 2024 implementation with direct GPIO control
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 * NOTE: Pin mapping to be determined based on schematic
 *       Currently using Close_IR_+ as placeholder for contactor control
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hvc_contactors.h"
#include "main.h"
#include "gpio.h"
#include "can.h" // for hvc_set_contactor_status
#include "hvc_state_machine.h"
#include "longhorn/rtos/logger.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

// TODO: Update these pin definitions based on actual HVC schematic
// These are placeholders based on visible GPIO output pins
#define PRECHARGE_CONTACTOR_PIN     GPIO_PIN_0  // Placeholder - update from schematic
#define PRECHARGE_CONTACTOR_PORT    GPIOB
#define AIR_PLUS_PIN                GPIO_PIN_6
#define AIR_PLUS_PORT               GPIOB

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static bool precharge_state = false;
static bool drive_state = false;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize contactor control
 */
void contactors_init(void) {
    // Ensure all contactors start in open state
    open_all_contactors();
    
    precharge_state = false;
    drive_state = false;
}

/**
 * @brief Set precharge contactor state
 */
void set_precharge_contactor(bool state) {
    GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(PRECHARGE_CONTACTOR_PORT, PRECHARGE_CONTACTOR_PIN, pin_state);
    precharge_state = state;
}

/**
 * @brief Set drive contactors state (both AIRs)
 */
void set_drive_contactors(bool state) {
    HAL_GPIO_WritePin(AIR_PLUS_PORT, AIR_PLUS_PIN, (GPIO_PinState)state);
    drive_state = state;
}

/**
 * @brief Emergency open all contactors
 */
void open_all_contactors(void) {
    set_precharge_contactor(false);
    set_drive_contactors(false);
}

/**
 * @brief Get precharge contactor state
 */
bool get_precharge_contactor_state(void) {
    return precharge_state;
}

/**
 * @brief Get drive contactors state
 */
bool get_drive_contactors_state(void) {
    return drive_state;
}

/*
 * Read shutdown/contactor sense lines and publish status over CAN.
 * NOTE: This assumes the sense lines are active-high (GPIO_PIN_SET == asserted).
 * Adjust inversion here if your hardware is active-low.
 */
void hvc_update_contactor_status(void)
{
    bool sense_state = get_drive_contactors_state();
    hvc_state_t state = get_current_state();
    log_printf(LOG_INFO, "HVC Contactors in update: %d\n", sense_state);
    hvc_set_contactor_status(state, sense_state, sense_state);
}
