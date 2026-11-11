#ifndef ORION_WHEEL_CALIBRATION_H
#define ORION_WHEEL_CALIBRATION_H
#include "wheel_phase.h"
/* Replace per corner with measured offset/amplitude/polarity and noise bounds.
 * Confirmed geometry does not establish these coefficients or sample timing. */
static inline wheel_phase_config_t orion_wheel_calibration(void) {
  wheel_phase_config_t config = wheel_phase_default_config();
  config.calibrated = false;
  return config;
}
#define ORION_WHEEL_ACQUISITION_TIMING_QUALIFIED 0
/* Measured delay from conversion epoch to software read-completion timestamp. */
#define ORION_WHEEL_CONVERSION_AGE_US 0u
#endif
