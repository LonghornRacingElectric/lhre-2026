#ifndef CSM_FIRMWARE_RIDE_HEIGHT_H
#define CSM_FIRMWARE_RIDE_HEIGHT_H

#include "main.h"

void ride_height_init();
float ride_height_get_distance_mm();
uint8_t ride_height_get_quality();


#endif // CSM_FIRMWARE_RIDE_HEIGHT_H