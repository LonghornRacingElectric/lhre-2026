//
// Created by rolan on 4/5/2025.
//

#ifndef VCT_SENSE_H
#define VCT_SENSE_H

#define COMMON_MODE_VOLTAGE 1.735052f
#define COMMON_MODE_CURRENT 1.649572f
#define VOLTAGE_CONSTANT (7.95f * 750.0f / 2000750.0f)
#define CURRENT_CONSTANT (8.5f * 0.0005f)
#define BETA 3950.0f  // Constant for 3590B Thermistor
#define R0   10000.0f // Resistance of Thermistor @ 25°C
#define T0_K 298.15f  // 25°C in Kelvin

float getTractiveCurrent();
float getTractiveVoltage();

float ntcToTemp(float r_ntc);
float getBusBar1Temp();
float getBusBar2Temp();
float getBusBar3Temp();
float getPrechargeTemp();

#endif //VCT_SENSE_H
