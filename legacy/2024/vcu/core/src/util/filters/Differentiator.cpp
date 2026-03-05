#include "Differentiator.h"

Differentiator::Differentiator() {
    this->last = 0.0f;
}

void Differentiator::reset() {
    this->last = 0.0f;
}

float Differentiator::get(float value, float deltaTime) {
    float dx = (value - this->last);
    float dx_dt = dx / deltaTime;
    this->last = value;
    return dx_dt;
}
