//
// Created by Kaitlyn Chang on 1/8/2025.
//

#ifndef CONTACTORS_H
#define CONTACTORS_H

#include <stdbool.h>

void contactors_init(void);
void setTractiveContactor(bool on);
bool isPosContactorClosed();
bool isNegContactorClosed();
bool isShutdownClosed(void);

#endif //CONTACTORS_H
