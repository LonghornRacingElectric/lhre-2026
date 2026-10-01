//
// Created by rolan on 4/5/2025.
//

#include "vct_sense.h"
#include "adc.h"

float getTractiveCurrent() {
  volatile float raw_isense_data = getISense();
  return (raw_isense_data - COMMON_MODE_CURRENT) / CURRENT_CONSTANT * 2;
}

float getTractiveVoltage() {
  volatile float raw_vsense_data = getVSense();
  return (raw_vsense_data - COMMON_MODE_VOLTAGE) / VOLTAGE_CONSTANT * 2;
}

float ntcToTemp(float r_ntc) {
  float inv_T = (1.0f / T0_K) + (1.0f / BETA) * log(r_ntc / R0);
  return (1.0f / inv_T) - 273.15f; // Convert to Celsius
}

float getBusBar1Temp() {
  float v_adc = getTempOne();
  float r_ntc = 10000.0f * (v_adc / (3.3f - v_adc));
  return ntcToTemp(r_ntc);
}

float getBusBar2Temp() {
  float v_adc = getTempTwo();
  float r_ntc = 10000.0f * (v_adc / (3.3f - v_adc));
  return ntcToTemp(r_ntc);
}

float getBusBar3Temp() {
  float v_adc = getTempThree();
  float r_ntc = 10000.0f * (v_adc / (3.3f - v_adc));
  return ntcToTemp(r_ntc);
}

float getPrechargeTemp() {
  float v_adc = getTempFour();
  float r_ntc = 10000.0f * (v_adc / (3.3f - v_adc));
  return ntcToTemp(r_ntc);
}

