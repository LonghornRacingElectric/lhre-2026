#include "charging.h"

#include "cells.h"
#include "faults.h"
#include "hvc_can.h"
#include "imd.h"

#define NUM_SERIES_CELLS 140.0f
#define CELL_MAX_VOLTAGE 4.2f
#define CELL_CV_START_VOLTAGE 4.1f
#define MAX_PACK_VOLTAGE (NUM_SERIES_CELLS * CELL_MAX_VOLTAGE)
#define MAX_CHARGE_CURRENT 9.5f

static float clamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void hvc_control_charging(bool enable) {
    const float maxCellVoltage = getMaxCellVoltage();
    const float taper = clamp(
        (CELL_MAX_VOLTAGE - maxCellVoltage) /
            (CELL_MAX_VOLTAGE - CELL_CV_START_VOLTAGE),
        0.0f, 1.0f);

    hvc_can_set_charger_command(MAX_PACK_VOLTAGE,
                                MAX_CHARGE_CURRENT * taper,
                                !isImdOk(),
                                get_latched_faults() != 0U,
                                enable);
}
