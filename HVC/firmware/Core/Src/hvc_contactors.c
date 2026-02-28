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

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static bool drive_state = false;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize contactor control
 */
void contactors_init(void) {
    // Ensure all contactors start in open state
    set_positive_contactor(false);
    drive_state = false;
}

/**
 * @brief Set positive contactor state (danger!)
 */
void set_positive_contactor(bool state) {
    HAL_GPIO_WritePin(Close_IR___GPIO_Port, Close_IR___Pin, (GPIO_PinState)state);
    drive_state = state;
}


/*
 * Read shutdown/contactor sense lines and publish status over CAN.
 * NOTE: This assumes the sense lines are active-high (GPIO_PIN_SET == asserted).
 * Adjust inversion here if your hardware is active-low.
 */
void hvc_update_contactor_status(void)
{
    bool sense_state_plus = HAL_GPIO_ReadPin(IR__Sense_GPIO_Port, IR__Sense_Pin) == GPIO_PIN_RESET;
    bool sense_state_minus = HAL_GPIO_ReadPin(IR__SenseC3_GPIO_Port, IR__SenseC3_Pin) == GPIO_PIN_RESET;
    hvc_state_t state = get_current_state();
    // log_printf(LOG_INFO, "HVC Contactors | State = %d | Sense IR+ = %d | Sense IR- =%d\n", state, sense_state_plus, sense_state_minus);
    hvc_set_contactor_status(state, sense_state_plus, sense_state_minus);
}
