//
// Created by Ashwin Kuppahally on 3/24/25.
//

#ifndef CHARGING_H
#define CHARGING_H
#include <stdbool.h>

void charging_init(int a);
void charging_periodic(float dt);
bool getBmsError();
bool getImdError();
bool getEnabled();
#endif //CHARGING_H
