#ifndef VCU_OUTPUTS_H
#define VCU_OUTPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  /* Normalized pedal travel */
  float apps1_travel;   // travel percentage of apps1
  float apps2_travel;   // travel percentage of apps2
  float pedal;          // middle of apps1 and apps2
  float pedal_filtered; // filtered pedal

  /* Torque command */
  float torque_cmd; // torque command in Nm

  /* Status flags */
  bool apps_implaus;  // true if APPS is implausible
  bool brake_active;  // true if brake is active
  bool brake_latched; // true if brake is latched

  /* Buzzer */
  bool buzzer_active; // true if buzzer is active

  /* Debug data */
  uint32_t apps_implaus_ms; // how long the APPS has been implausible
  float apps_diff;          // difference between apps1 and apps2

  /* BSE pressure estimate (psi) */
  float bse_psi; // brake pressure in psi

} vcu_outputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_OUTPUTS_H */
