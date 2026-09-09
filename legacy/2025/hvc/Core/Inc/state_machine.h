//
// Created by rolan on 4/6/2025.
//

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>

#define STATE_NOT_ENERGIZED 1
#define STATE_PRECHARGING 2
#define STATE_ENERGIZED 3
#define STATE_CHARGING 4

void state_machine_init();
int update_state_machine(bool shutdownClosed, bool hvOk, bool chargerPresent, float deltaTime);

#endif //STATE_MACHINE_H
