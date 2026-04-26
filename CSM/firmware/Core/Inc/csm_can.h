#ifndef CSM_CAN_H
#define CSM_CAN_H

#include <stdint.h>

void csm_can_init(void);
void csm_can_update_strain_gauge_sus_pot(float suspot_travel, float strain_voltage);
void csm_can_update_accel_ride_height(float x, float y, float z, float ride_height_mm);

#endif // CSM_CAN_H