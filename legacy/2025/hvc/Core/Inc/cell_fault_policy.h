#ifndef CELL_FAULT_POLICY_H
#define CELL_FAULT_POLICY_H

#include <stdbool.h>

#define HVC_HIGH_IMPEDANCE_SUSPECT_LOW_MAX_V 1.0f
#define HVC_HIGH_IMPEDANCE_SUSPECT_HIGH_MIN_V 5.0f

bool cell_fault_is_high_impedance_suspect(bool bmbCommunicationOk,
                                          float measuredVoltageV);
bool cell_fault_is_undervoltage(bool bmbCommunicationOk,
                                float measuredVoltageV,
                                float undervoltageThresholdV,
                                bool ignoreHighImpedanceSuspects);
bool cell_fault_is_overvoltage(bool bmbCommunicationOk,
                               float measuredVoltageV,
                               float overvoltageThresholdV,
                               bool ignoreHighImpedanceSuspects);

#endif // CELL_FAULT_POLICY_H
