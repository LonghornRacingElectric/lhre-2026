#include "cell_fault_policy.h"

bool cell_fault_is_high_impedance_suspect(bool bmbCommunicationOk,
                                          float measuredVoltageV)
{
    /* A normally connected Li-ion cell cannot plausibly sit below 1 V or
       above 5 V during this pack bring-up. Only apply the classification when
       the BMB itself passed its PEC/communication checks. */
    return bmbCommunicationOk &&
           (measuredVoltageV < HVC_HIGH_IMPEDANCE_SUSPECT_LOW_MAX_V ||
            measuredVoltageV > HVC_HIGH_IMPEDANCE_SUSPECT_HIGH_MIN_V);
}

bool cell_fault_is_undervoltage(bool bmbCommunicationOk,
                                float measuredVoltageV,
                                float undervoltageThresholdV,
                                bool ignoreHighImpedanceSuspects)
{
    if (measuredVoltageV >= undervoltageThresholdV) return false;

    if (ignoreHighImpedanceSuspects &&
        cell_fault_is_high_impedance_suspect(bmbCommunicationOk,
                                             measuredVoltageV)) {
        return false;
    }

    return true;
}

bool cell_fault_is_overvoltage(bool bmbCommunicationOk,
                               float measuredVoltageV,
                               float overvoltageThresholdV,
                               bool ignoreHighImpedanceSuspects)
{
    if (measuredVoltageV <= overvoltageThresholdV) return false;

    if (ignoreHighImpedanceSuspects &&
        cell_fault_is_high_impedance_suspect(bmbCommunicationOk,
                                             measuredVoltageV)) {
        return false;
    }

    return true;
}
