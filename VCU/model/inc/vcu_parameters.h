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
    uint16_t apps1_min_adc;
    uint16_t apps1_max_adc;
    uint16_t apps2_min_adc;
    uint16_t apps2_max_adc;
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
    float max_torque_nm; // maximum torque request allowed in Nm
  } torque_map;
} vcu_parameters_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_PARAMETERS_H */
