#include "Debounce.h"

Debounce::Debounce(float duration, bool initialState) {
    this->timer = Timer(duration);
    this->state = initialState;
}

void Debounce::add(bool input, float deltaTime) {
    if(state != input) {
        timer.count(deltaTime);
        if(timer.isFinished()) {
            timer.reset();
            state = input;
        }
    } else {
        timer.reset();
    }
}

bool Debounce::get() {
    return state;
}


