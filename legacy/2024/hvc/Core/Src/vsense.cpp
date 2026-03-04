//
// Created by rolandwang on 11/12/2023.
// Developed by angelasrsh on 3/3/24.
//

#include "vsense.h"
#include "adc.h"

static float MAX_VOLTAGE = 588.0f;

float getTractiveVoltage() {
  //insert calculations needed
  volatile float VSenseVoltage = getVSense();

  /* theory of operation:
   * HV_inputV * (R2/R2+R1) = V_isoAmp
   * V_isoAmp * 8.2 = V_mcu = VSenseVoltage
   * note derek updated because gain is really 4.11 and there's an offset apparently
   *
   * Note: at 588V, output should be 1.6318 V give or take a few mV
   */

//    return (float) ((VSenseVoltage-1.27f) / 4.11f) * (2942.176473f); // 2942.176473 is voltage divider ratio
  return (VSenseVoltage - 1.25f) * 715.9;

}

