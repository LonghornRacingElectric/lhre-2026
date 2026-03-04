#ifndef VCU_CORE_DEBOUNCE_H
#define VCU_CORE_DEBOUNCE_H


#include "Timer.h"

class Debounce {
private:
    bool state;
    Timer timer = Timer(0);
public:
    Debounce(float duration, bool initialState);
    void add(bool input, float deltaTime);
    bool get();
};


#endif //VCU_CORE_DEBOUNCE_H
