#include "lvbms_bluetooth.h"
#include "usart.h"
#include "stdint.h"
#include "stdio.h"
#include <string.h>

static UART_HandleTypeDef *ble_uart;

void RN4781_Init(UART_HandleTypeDef *huart)
{
    ble_uart = huart;

    HAL_Delay(100);

    // Enter Command Mode
    HAL_UART_Transmit(huart, (uint8_t *)"$$$", 3, HAL_MAX_DELAY);
    HAL_Delay(200); // wait for CMD> prompt

    // Set device name
    RN4781_SendCommand("SN,LVBMS");

    // Enable Device Info + UART Transparent service
    RN4781_SendCommand("SS,C0");

    // Ensure connectable advertising
    RN4781_SendCommand("SC,0");

    // Reboot module to apply
    RN4781_SendCommand("R,1");
    HAL_Delay(2000); // wait for module to restart and advertise
}

void RN4781_SendCommand(const char *cmd)
{
    HAL_UART_Transmit(ble_uart, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    HAL_UART_Transmit(ble_uart, (uint8_t *)"\r", 1, HAL_MAX_DELAY); // Commands require \r
    HAL_Delay(100); // small delay for module to respond
}

void BLE_Send(const char *msg)
{
    if (!msg) return;
    HAL_UART_Transmit(ble_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}