#include "LowPassFilter.h"

LowPassFilter::LowPassFilter(float timeConstant) {
    this->timeConstant = timeConstant;
    this->accumulator = 0;
}

void LowPassFilter::reset() {
    this->accumulator = 0;
}

void LowPassFilter::add(float value, float deltaTime) {
    float alpha = deltaTime / (timeConstant + deltaTime);
    accumulator = (accumulator * (1-alpha)) + (value * alpha);
}

float LowPassFilter::get() {
    return accumulator;
}
