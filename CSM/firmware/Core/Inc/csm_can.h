#ifndef CSM_CAN_H
#define CSM_CAN_H

#include <stdint.h>


void csm_can_init(void);
void csm_can_update_strain_gauge_sus_pot(float suspot_travel, float strain_voltage);
#endif // CSM_CAN_H