#ifndef VCU_PARAMETERS_H
#define VCU_PARAMETERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
  float brake_enable_threshold;
  uint16_t buzzer_duration_ms;

  struct {
    float apps1_min_adc_v;
    float apps1_max_adc_v;
    float apps2_min_adc_v;
    float apps2_max_adc_v;
    float min_travel_threshold;         // the amount of travel required to be
                                        // considered pressed
    float max_travel_restore_threshold; // the amount of travel required to
                                        // restore APPS from implausible state
    float max_allowable_diff; // maximum difference allowed between both APPS
                              // sensors
    uint16_t implaus_debounce_time_ms; // how long the APPS must be implausible
                                       // before torque must be cut
  } apps;

  struct {
    // Software Hystersis for BSE
    float bse_off_psi; // pressure at which brake is considered off
    float bse_on_psi;  // pressure at which brake is considered on

    float bse_adc_at_min_psi_v;       // ADC reading at the lowest BSE value
    float bse_adc_at_max_psi_v;       // ADC reading at the highest BSE value
    float bse_max_psi;                 // maximum pressure reading possible
    float max_pedal_while_braking;     // maximum pedal allowed while braking
    float max_pedal_restore_threshold; // maximum pedal allowed to restore BSE
  } bse;

  struct {
    float max_torque_nm; // maximum torque request allowed in Nm
  } torque_map;
} vcu_parameters_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_PARAMETERS_H */
