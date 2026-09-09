//
// Created by rolan on 4/4/2025.
//

#include "hvc_can.h"

#include "cells.h"
#include "fdcan.h"
#include "soc_estimation.h"
#include "vct_sense.h"

NightCANInstance can1;
static NightCANReceivePacket hvcParameterRX;
static NightCANReceivePacket allowBalanceRX;
static NightCANPacket packStatus;
static NightCANPacket tempStatus;
static NightCANPacket indicatorStatus;
static NightCANPacket contactorStatus;

void hvc_can_init() {
    can1 = CAN_new_instance();
    CAN_Init(&can1, &hfdcan1, 0, 0xFF, 0, 0);

    hvcParameterRX = CAN_create_receive_packet(0x010, 1000, 8);
    allowBalanceRX = CAN_create_receive_packet(0x114, 1000, 8);
    CAN_addReceivePacket(&can1, &hvcParameterRX);
    CAN_addReceivePacket(&can1, &allowBalanceRX);

    packStatus = CAN_create_packet(0x220, 100, 8);
    tempStatus = CAN_create_packet(0x221, 100, 4);
    indicatorStatus = CAN_create_packet(0x222, 100, 2);
    contactorStatus = CAN_create_packet(0x223, 100, 1);
    CAN_AddTxPacket(&can1, &packStatus);
    CAN_AddTxPacket(&can1, &tempStatus);
    CAN_AddTxPacket(&can1, &indicatorStatus);
    CAN_AddTxPacket(&can1, &contactorStatus);
}

void hvc_can_periodic(bool amsError, bool imdError, int state, float deltaTime) {
    // Pack Status Data
    CAN_writeFloat(uint16_t, &packStatus, 0, getPackVoltageFromCells(), 0.01f);
    CAN_writeFloat(uint16_t, &packStatus, 2, getTractiveCurrent(), 0.01f);
    CAN_writeFloat(uint16_t, &packStatus, 4, getSoc(deltaTime), 0.01f);
    CAN_writeFloat(uint8_t, &packStatus, 6, getMaxTemp(), 0.1f); // change to top of cell temp
    CAN_writeFloat(uint8_t, &packStatus, 7, getMinTemp(), 0.1f); // change to bot of cell temp

    // Temperature Status Data
    CAN_writeFloat(uint16_t, &tempStatus, 0, getBusBar1Temp(), 0.1f);
    CAN_writeFloat(uint16_t, &tempStatus, 1, getBusBar2Temp(), 0.1f);
    CAN_writeFloat(uint16_t, &tempStatus, 2, getBusBar3Temp(), 0.1f);
    CAN_writeFloat(uint16_t, &tempStatus, 3, getPrechargeTemp(), 0.1f);

    // Indicator Status Data
    CAN_writeInt(uint8_t, &indicatorStatus, 0, amsError);
    CAN_writeInt(uint8_t, &indicatorStatus, 1, imdError);

    // Contactor Status Data
    CAN_writeInt(uint8_t, &contactorStatus, 0, state);

    // HVC Parameter Data
    if(hvcParameterRX.is_recent) {
        hvcParameterRX.is_recent = false;
        float underVoltage = CAN_readFloat(uint16_t, &hvcParameterRX, 0, 0.1f);
        float overVoltage = CAN_readFloat(uint16_t, &hvcParameterRX, 2, 0.1f);
        float underTemp = CAN_readFloat(uint16_t, &hvcParameterRX, 4, 1.0f);
        float overTemp = CAN_readFloat(uint16_t, &hvcParameterRX, 6, 1.0f);
        updateBmsLimits(underVoltage, overVoltage, underTemp, overTemp);
    }

    // Allow Balance Data
    if(allowBalanceRX.is_recent) {
        allowBalanceRX.is_recent = false;
        carParked = CAN_readInt(uint8_t, &allowBalanceRX, 0);
    }
    else if(allowBalanceRX.is_recent) {
        carParked = false;
    }
}