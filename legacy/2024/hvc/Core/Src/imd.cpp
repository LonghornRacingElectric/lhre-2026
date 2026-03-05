//
// Created by rolandwang on 11/12/2023.
//

#include "imd.h"
#include "main.h"

bool isImdOk() {
    return HAL_GPIO_ReadPin(IMD_Data_GPIO_Port, IMD_Data_Pin) == GPIO_PIN_SET;
}