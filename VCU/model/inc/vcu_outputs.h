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
  float pedal;          // middle of apps1 and apps2 in percent
  float pedal_filtered; // filtered pedal

  /* Torque command */
  float torque_cmd; // torque command in Nm

  /* Status flags */
  bool brake_active; // true if brake is active

  uint8_t prndl_state; // current state of the PRNDL machine

  /* Buzzer */
  bool buzzer_active; // true if buzzer is active

  /* BSE pressure estimate (psi) */
  float bse_psi; // brake pressure in psi

  struct {
    bool apps1_under_range;
    bool apps1_over_range;
    bool apps2_under_range;
    bool apps2_over_range;
    bool apps_implaus;
    bool apps_any_fault;

    bool brake_latched;
    bool brake_any_fault;

    bool any_fault;
  } faults;

  /* Debug data */
  struct {
    uint32_t apps_implaus_ms; // how long the APPS has been implausible
    float apps_diff;          // difference between apps1 and apps2
  } debug;

} vcu_outputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_OUTPUTS_H */
