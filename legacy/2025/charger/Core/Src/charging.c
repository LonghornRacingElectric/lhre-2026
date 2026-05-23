//
// Created by Ashwin Kuppahally on 3/24/25.
//

#include "charging.h"

#include <stdio.h>
#include <stdbool.h>
#include "can.h"
#include "led.h"
#include "pilot.h"

//Can 1 is Elcon
static CAN_HandleTypeDef *elcon=&hcan1;

//Can 2 is HVC
static CAN_HandleTypeDef *hvc=&hcan2;

extern int rainbow;

static int angelique = 0;

static int nightwatch = 0;

static uint16_t rawVoltageRequestData = 0;
static uint16_t rawCurrentRequestData = 0;
static uint8_t carYear = 0;
static uint8_t amsError = 0;
static uint8_t imdError = 0;
static uint8_t hvcState = 0;

void charging_init(int a)
{
    if(a == 1) {
        angelique = a;
    } else if (a == 2) {
        nightwatch = a;
    }

    CAN_FilterTypeDef sFilterConfigHVC;

    // Select filter bank and configure it
    sFilterConfigHVC.FilterBank = 14;                  // Filter Bank (0 to 13 for single CAN)
    sFilterConfigHVC.FilterMode = CAN_FILTERMODE_IDMASK; // Use Mask Mode for ID range
    sFilterConfigHVC.FilterScale = CAN_FILTERSCALE_32BIT; // 32-bit scale for Extended IDs

    // Set the Extended ID (29-bit) - Left-align it for the hardware register
    // Conficured for the ChargingLimits Can ID
    // sFilterConfigHVC.FilterIdHigh = (0x1806E5F4 >> 13) & 0xFFFF; // Upper 16 bits
    // sFilterConfigHVC.FilterIdLow = (0x1806E5F4 << 3) & 0xFFF8;   // Lower 13 bits + IDE bit

    sFilterConfigHVC.FilterIdHigh = (0x1806E5F4 >> 13) & 0xFFFF;
    sFilterConfigHVC.FilterIdLow = (0x1806E5F4 << 3) & 0xFFF8;

    // Mask - Match all 29-bits (0x1FFFFFFF)
    sFilterConfigHVC.FilterMaskIdHigh = (0x1FFFFFFF >> 13) & 0xFFFF;
    sFilterConfigHVC.FilterMaskIdLow = (0x1FFFFFFF << 3) & 0xFFF8;

    // Assign to a FIFO (CAN_RX_FIFO0 or CAN_RX_FIFO1)
    sFilterConfigHVC.FilterFIFOAssignment = CAN_RX_FIFO1;

    // Enable the filter
    sFilterConfigHVC.FilterActivation = ENABLE;

    // Required for dual CAN (or set 14 for SlaveStartFilterBank on single CAN)
    // sFilterConfigHVC.SlaveStartFilterBank = 12;

    // Apply the filter configuration
    if (HAL_CAN_ConfigFilter(hvc, &sFilterConfigHVC) != HAL_OK) {
        Error_Handler();
    }




    CAN_FilterTypeDef sFilterConfigElcon;

    // Select filter bank and configure it
    sFilterConfigElcon.FilterBank = 0;                  // Filter Bank (0 to 13 for single CAN)
    sFilterConfigElcon.FilterMode = CAN_FILTERMODE_IDMASK; // Use Mask Mode for ID range
    sFilterConfigElcon.FilterScale = CAN_FILTERSCALE_32BIT; // 32-bit scale for Extended IDs

    // Set the Extended ID (29-bit) - Left-align it for the hardware register
    // Conficured for the Status Can ID
    sFilterConfigElcon.FilterIdHigh = (0x18FF50E5 >> 13) & 0xFFFF; // Upper 16 bits
    sFilterConfigElcon.FilterIdLow = (0x18FF50E5 << 3) & 0xFFF8;   // Lower 13 bits + IDE bit

    // Mask - Match all 29-bits (0x1FFFFFFF)
    sFilterConfigElcon.FilterMaskIdHigh = (0x1FFFFFFF >> 13) & 0xFFFF;
    sFilterConfigElcon.FilterMaskIdLow = (0x1FFFFFFF << 3) & 0xFFF8;

    // Assign to a FIFO (CAN_RX_FIFO0 or CAN_RX_FIFO1)
    sFilterConfigElcon.FilterFIFOAssignment = CAN_RX_FIFO0;

    // Enable the filter
    sFilterConfigElcon.FilterActivation = ENABLE;

    // Required for dual CAN (or set 14 for SlaveStartFilterBank on single CAN)
    // sFilterConfigElcon.SlaveStartFilterBank = 13;

    // Apply the filter configuration
    if (HAL_CAN_ConfigFilter(elcon, &sFilterConfigElcon) != HAL_OK) {
        Error_Handler();
    }


    HAL_CAN_Start(elcon);
    HAL_CAN_Start(hvc);

}
void angeliqueLoop(float dt)
{
    static CAN_TxHeaderTypeDef TxHeader;
    static CAN_RxHeaderTypeDef RxHeader;
    static CAN_TxHeaderTypeDef TxToHVC;
    static uint8_t chargerPluggedPacket[8] = {1};
    static uint8_t statusData[8] = {0};
    static uint8_t commandData[8] = {0};

    // Hard-coded Elcon charge command:
    //   Bytes 0-1: max voltage = 5100 (510.0V x 0.1) -- set high, charger's CV mode will cap it
    //   Bytes 2-3: max current = 100  (10.0A  x 0.1) -- change 0x64 to desired amps x 10
    //   Byte  4:   0x00 = charging enabled (0x01 = stop)
    static uint8_t tempCommandData[8] = {0x14, 0x82, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00}; // 525.0V, 2.5A

    // carYear = commandData[0];
    // rawVoltageRequestData = commandData[1] << 8 | commandData[2];
    // rawCurrentRequestData = commandData[3] << 8 | commandData[4];
    // amsError = commandData[5];
    // imdError = commandData[6];
    // hvcState = commandData[7];

    static float nextTime = 0;
    static float currTime = 0;

    /*

    while (HAL_CAN_GetRxFifoFillLevel(hvc, CAN_RX_FIFO1)>0)
    {
        RxHeader.ExtId = 0;
        uint32_t error = HAL_CAN_GetRxMessage(hvc, CAN_RX_FIFO1, &RxHeader, commandData);
        if (error != HAL_OK) {
            Error_Handler();
        }
        if (RxHeader.StdId != 0x2FE)
        {
            Error_Handler();
        }

        carYear = commandData[0];
        rawVoltageRequestData = commandData[1] << 8 | commandData[2];
        rawCurrentRequestData = commandData[3] << 8 | commandData[4];
        amsError = commandData[5];
        imdError = commandData[6];
        hvcState = commandData[7];

        // override the voltage command
        // commandData[0]= 5100 / 256;
        // commandData[1]= 5100 % 256;


        // float curLimit=getCurrentLimit();
        //
        // // CAN override HVC requested current here
        // commandData[2]=0;
        // commandData[3]= (int) (curLimit*10);  // overriding to 10.0A for ChargePoint, 2.5 for wall charger so we dont blow up pickle
        //
        // rainbow |= 0x01;
    }

*/

    while (HAL_CAN_GetRxFifoFillLevel(elcon, CAN_RX_FIFO0)>0)
    {
        RxHeader.ExtId = 0;
        uint32_t error = HAL_CAN_GetRxMessage(elcon, CAN_RX_FIFO0, &RxHeader, statusData);
        if (error != HAL_OK) {
            Error_Handler();
        }

        if (RxHeader.ExtId != 0x18FF50E5)
        {
            Error_Handler();
        }
    }

    currTime += dt;
    if(currTime > nextTime)
    {
        nextTime += 0.05f;  // 20 Hz

        // relay charger plugged in packet
        TxHeader.StdId=0x114;
        TxHeader.RTR=CAN_RTR_DATA;
        TxHeader.DLC=8;
        TxHeader.IDE=CAN_ID_STD;
        uint32_t mailbox2 = 0;
        uint32_t error3 = HAL_CAN_AddTxMessage(elcon, &TxHeader, chargerPluggedPacket, &mailbox2);
       // uint32_t error3 = HAL_CAN_AddTxMessage(elcon, &TxToHVC, chargerPluggedPacket, &mailbox2);
        if (error3 != HAL_OK) {
            Error_Handler();
        }

        // relay command to elcon
        // if(rainbow & 0x01)
        // {
            TxHeader.ExtId=0x1806E5F4;
            TxHeader.RTR=CAN_RTR_DATA;
            TxHeader.DLC=8;
            TxHeader.IDE=CAN_ID_EXT;
            uint32_t mailbox=0;
            uint32_t error2=HAL_CAN_AddTxMessage(elcon, &TxHeader, tempCommandData, &mailbox);
            if (error2 != HAL_OK)
            {
                Error_Handler();
            }
        // }


        // relay status to hvc
        if(rainbow & 0x02)
        {
            TxHeader.ExtId=0x18FF50E5;
            TxHeader.RTR=CAN_RTR_DATA;
            TxHeader.DLC=8;
            TxHeader.IDE=CAN_ID_EXT;
            uint32_t mailbox=0;
            uint32_t error2=HAL_CAN_AddTxMessage(hvc, &TxHeader, statusData, &mailbox);
            if (error2 != HAL_OK)
            {
                Error_Handler();
            }
        }
    }
}

/**
 * Loop to receive and send CAN
 */
void charging_periodic(float dt)
{
    //TODO: add detection for prox signal to enable charging

    //old head detected
    //if (nightwatch == 2)
    //{
        angeliqueLoop(dt);
    //}
}

bool getAmsError() {
    return amsError;
}

bool getImdError() {
    return imdError;
}