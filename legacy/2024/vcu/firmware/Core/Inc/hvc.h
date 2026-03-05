#ifndef VCU_FIRMWARE_2024_HVC_H
#define VCU_FIRMWARE_2024_HVC_H

#include <cstdint>
#include "VcuModel.h"
#include "LonghornLib/angel_can.h"

#define VOLTS_MAILBOXES_NUM (HVC_VCU_CELL_VOLTAGES_END - HVC_VCU_CELL_VOLTAGES_START + 1)
#define TEMPS_MAILBOXES_NUM (HVC_VCU_CELL_TEMPS_END - HVC_VCU_CELL_TEMPS_START + 1)

typedef struct HvcStatus {
    bool isRecent;

    float packVoltage;
    float packCurrent;
    float stateOfCharge;

    //Stats for voltage vals
    float packVoltageMean;
    float packVoltageMin;
    float packVoltageMax;
    float packVoltageRange;

    // Stats for temp vals
    float packTempMean;
    float packTempMin;
    float packTempMax;
    float packTempRange;

    bool imd;
    bool ams;

    uint8_t contactorStatus;

    // TODO cell voltages and temps
    float cellVoltages[4*VOLTS_MAILBOXES_NUM] = {0};
    float cellTemps[4*TEMPS_MAILBOXES_NUM] = {0};
} HvcStatus;

/**
 * Initialize CAN communication with the HVC by subscribing to certain CAN IDs for example.
 */
void hvc_init();

/**
 * Update CAN packet to set cooling fan status and speed if needed.
 * @param battFanRpmPercentage the desired RPM % of the cooling fan cooling the battery segments
 * @param battUniqueSegRpmPercentage the desired RPM % of the cooling fan cooling the unique segment of the battery
 */
static void hvc_updateCooling(float battFanRpmPercentage, float battUniqueSegRpmPercentage);

/**
 * Checks CAN mailboxes and updates status accordingly. Also updates cooling output CAN packet.
 */
void hvc_periodic(HvcStatus* status, VcuOutput* vcuOutput);


#endif //VCU_FIRMWARE_2024_HVC_H
