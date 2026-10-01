#ifndef CELL_FAULT_POLICY_H
#define CELL_FAULT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* Temporary 2025 pack bring-up exclusions. Cell numbers are one-based in
 * terminal output; these constants are zero-based array indices. */
#define HVC_IGNORED_CELL_FIRST_INDEX 13U
#define HVC_IGNORED_CELL_LAST_INDEX 16U

/* Plausible bring-up range for a connected pack thermistor. Open/high-Z
 * channels have appeared as -999 C or implausible negative temperatures. A
 * reading above 60 C remains valid so it can raise the overtemperature fault. */
#define HVC_MIN_VALID_TEMPERATURE_C 0.0f
#define HVC_MAX_VALID_TEMPERATURE_C 135.0f

bool cell_fault_is_monitored(uint32_t cellIndex);
bool thermistor_reading_is_valid(float temperatureC);
bool thermistor_reading_is_overtemperature(float temperatureC,
                                            float overtemperatureLimitC);

#endif // CELL_FAULT_POLICY_H
