//
// Created by rolandwang on 11/12/2023.
//

#include "contactors.h"
#include "main.h"

void setPrechargeContactor(bool on) {
    HAL_GPIO_WritePin(Precharge_Enable_GPIO_Port, Precharge_Enable_Pin, (GPIO_PinState) on);
}

void setDriveContactor(bool on) {
    HAL_GPIO_WritePin(Close_HV_P_Signal_GPIO_Port, Close_HV_P_Signal_Pin, (GPIO_PinState) on);
    HAL_GPIO_WritePin(Close_HV_N_Signal_GPIO_Port, Close_HV_N_Signal_Pin, (GPIO_PinState) on);
}