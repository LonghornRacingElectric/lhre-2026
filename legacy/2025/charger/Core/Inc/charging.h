//
// Created by Ashwin Kuppahally on 3/24/25.
//

#ifndef CHARGING_H
#define CHARGING_H
#include <stdbool.h>

void charging_init(int a);
void charging_periodic(float dt);
bool getAmsError();
bool getImdError();
#endif //CHARGING_H
