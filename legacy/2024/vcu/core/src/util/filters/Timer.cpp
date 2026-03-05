#include "Timer.h"

Timer::Timer(float duration) {
    this->duration = duration;
    time = duration;
}

void Timer::reset() {
    time = duration;
}

void Timer::count(float deltaTime) {
    time -= deltaTime;
}

bool Timer::isFinished() {
    return time <= 0;
}
