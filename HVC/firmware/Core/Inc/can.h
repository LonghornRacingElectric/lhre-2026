#ifndef __CAN_H__
#define __CAN_H__

#include <stdbool.h>

void hvc_can_init(void);

void hvc_set_contactor_status(int state, bool pos, bool neg);
void hvc_set_indicator_status(bool bms_error, bool imd_error,
                              bool shutdown_leg1, bool shutdown_leg2,
                              bool shutdown_leg3, bool shutdown_leg4);
void hvc_set_min_cell_voltage(float min_cell_voltage_v);
void hvc_set_max_cell_voltage(float max_cell_voltage_v);

void hvc_set_cell_temperatures(float *cell_temps);

void hvc_set_cell_voltages(float *cell_voltages);
#endif
