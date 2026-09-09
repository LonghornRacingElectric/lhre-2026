//
// Created by rolan on 4/4/2025.
//

#ifndef HVC_CAN_H
#define HVC_CAN_H

#include "night_can.h"
#include "night_can.h"
extern NightCANInstance can1;

void hvc_can_init();

void hvc_can_periodic(bool amsError, bool imdError, int state, float deltaTime);

#endif //HVC_CAN_H
