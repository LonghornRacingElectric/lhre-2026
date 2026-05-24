#ifndef LVBMS_BLUETOOTH_H
#define LVBMS_BLUETOOTH_H

#include "stm32g4xx_hal.h"

// Public functions
void RN4781_Init(UART_HandleTypeDef *huart);
void RN4781_SendCommand(const char *cmd);
void BLE_Send(const char *msg);

#endif