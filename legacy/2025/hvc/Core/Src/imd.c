//
// Created by Gautham Ramanarayanan on 2025/01/09.
//

#include "imd.h"

#include "fdcan.h"
#include "main.h"
#include "hvc_can.h"

NightCANInstance can3;
static NightCANPacket imdRequestPacket;
static NightCANReceivePacket imdResponsePacket;
static NightCANReceivePacket imdInfoGeneral;
static NightCANReceivePacket imdInfoIsoDetail;
static NightCANReceivePacket imdInfoVoltage;
static NightCANReceivePacket imdInfoITSystem;

bool isImdOk() {
    return HAL_GPIO_ReadPin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin) == GPIO_PIN_SET;
}

// Only use if GPIO is set to output (used for testing only)
void testSetIMD(bool error) {
    HAL_GPIO_WritePin(IMD_ERROR_GPIO_Port, IMD_ERROR_Pin, error);
}

void imd_can_init() {
    can3 = CAN_new_instance();
    CAN_Init(&can3, &hfdcan3, 0, 0xFF, 0, 0);

    imdRequestPacket = CAN_create_packet(IMD_REQUEST, 0, 8);
    imdResponsePacket = CAN_create_receive_packet(IMD_RESPONSE, 1000, 8);
    imdInfoGeneral = CAN_create_receive_packet(IMD_INFO_GENERAL, 1000, 8);
    imdInfoIsoDetail = CAN_create_receive_packet(IMD_INFO_ISO_DETAIL, 1000, 8);
    imdInfoVoltage = CAN_create_receive_packet(IMD_INFO_VOLTAGE, 1000, 8);
    imdInfoITSystem = CAN_create_receive_packet(IMD_INFO_IT_SYS, 1000, 8);

    CAN_AddTxPacket(&can1, &imdRequestPacket);
    CAN_addReceivePacket(&can3, &imdResponsePacket);
    CAN_addReceivePacket(&can3, &imdInfoGeneral);
    CAN_addReceivePacket(&can3, &imdInfoIsoDetail);
    CAN_addReceivePacket(&can3, &imdInfoVoltage);
    CAN_addReceivePacket(&can3, &imdInfoITSystem);
}

void imd_can_periodic() {

}

void imd_can_config() {

}