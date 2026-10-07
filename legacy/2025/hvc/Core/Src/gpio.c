/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, BMS_ERROR_Pin|CLOSE_IR_POS_Pin, GPIO_PIN_RESET);

  /* PB1 is the only driver of the IMD SR-latch set input (1 = fault).
     Preload it low (no fault) before it becomes an output. */
  HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_BMB_GPIO_Port, CS_BMB_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BOOT0trig_GPIO_Port, BOOT0trig_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PCPin PCPin PCPin PCPin
                           PCPin PCPin */
  GPIO_InitStruct.Pin = IR_POS_SENSE_Pin|IR_NEG_SENSE_Pin|SHDN_SENSE_1_Pin|SHDN_SENSE_2_Pin
                          |SHDN_SENSE_3_Pin|SHDN_SENSE_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Shutdown_Sense_12 is high when 24 V is present at the HV contactors. */
  GPIO_InitStruct.Pin = Shutdown_Sense_12_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Shutdown_Sense_12_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PBPin PBPin */
  GPIO_InitStruct.Pin = BMS_ERROR_Pin|CLOSE_IR_POS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = IMD_ERROR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IMD_ERROR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = CS_BMB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(CS_BMB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = BOOT0trig_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BOOT0trig_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */
bool isShutdownOneOk() {
    return HAL_GPIO_ReadPin(SHDN_SENSE_1_GPIO_Port, SHDN_SENSE_1_Pin) == GPIO_PIN_SET;
}

bool isShutdownTwoOk() {
    return HAL_GPIO_ReadPin(SHDN_SENSE_2_GPIO_Port, SHDN_SENSE_2_Pin) == GPIO_PIN_SET;
}

bool isShutdownThreeOk() {
    return HAL_GPIO_ReadPin(SHDN_SENSE_3_GPIO_Port, SHDN_SENSE_3_Pin) == GPIO_PIN_SET;
}

bool isShutdownFourOk() {
    return HAL_GPIO_ReadPin(SHDN_SENSE_4_GPIO_Port, SHDN_SENSE_4_Pin) == GPIO_PIN_SET;
}

bool isShutdownTwelveOk(void) {
    return HAL_GPIO_ReadPin(Shutdown_Sense_12_GPIO_Port,
                            Shutdown_Sense_12_Pin) == GPIO_PIN_SET;
}

void setBmsError(bool error) {
    HAL_GPIO_WritePin(BMS_ERROR_GPIO_Port, BMS_ERROR_Pin, error);
}
/* USER CODE END 2 */
