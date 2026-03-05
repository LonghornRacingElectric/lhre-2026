#ifndef VCU_FIRMWARE_2024_PDU_H
#define VCU_FIRMWARE_2024_PDU_H

#include <cstdint>
#include "VcuModel.h"

enum BuzzerType {
    BUZZER_SILENT = 0,
    BUZZER_BUZZ = 1,
    BUZZER_DEEP_IN_THE_HEART_OF_TEXAS = 2,
};

typedef struct PduStatus {
    float volumetricFlowRate;
    float waterTempInverter;
    float waterTempMotor;
    float waterTempRadiator;

    float radiatorFanRpm;

    float lvVoltage;
    float lvSoC;
    float lvCurrent;

    // Currents, need more info on this
    float treetrunk;
    float radfan;
    float battfan;
    float pump1;
    float pump2;
    float glv;
    float shdn;
    float bl;

    bool isRecent = false;
} PduStatus;

/**
 * Initialize the PDU's CAN mailboxes and wire signals
 */
void pdu_init();

static void pdu_updateBrakeLight(float brightnessLeft, float brightnessRight);

static void pdu_updateBuzzer(BuzzerType buzzerType);

static void pdu_updateCoolingOutput(uint16_t radiatorFanRpm, float pumpPercentage);

void pdu_periodic(PduStatus* status, VcuOutput* vcuOutput);


#endif //VCU_FIRMWARE_2024_PDU_H
