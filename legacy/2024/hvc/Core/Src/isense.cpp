//
// Created by rolandwang on 11/12/2023.
// Developed by angelasrsh on 1/8/24.
//

#include "isense.h"
#include "adc.h"
//checks if current is within max to open shutdown in case if curr too high
bool isPackCurrentWithinBounds() {
    float maxCurrSetting = 250.0f;
    if (getPackCurrent() > maxCurrSetting) {
        return false;
    }
    return true;
}

float calculateHallCurrent(float hallEffectVoltage, float sensitivity) {
    // current is measured by I = ((5/(supply voltage = Uc)) * Vout - (Voffset = 2.5)) * 1/(sensitivity)
    // sensitivity in volts per amp
    // negative because current direction is backwards
    return (hallEffectVoltage - 2.5f) / -sensitivity;
}

//Returns the current rating measured directly (mostly HighCurrVal)
//±50 A for channel 1
//±300 A for channel 2
float getPackCurrent() {
    // current is measured by I = ((5/(supply voltage = Uc)) * Vout - Voffset) * 1/(sensitivity = 6.67)
    float lowCurr = calculateHallCurrent(getISenseLow(), 0.040f);
    float highCurr = calculateHallCurrent(getISenseHigh(), 0.00667f);

    return highCurr;

    // TODO low current wire appears to be loose?
    if (highCurr >= 50.0f || highCurr <= -50.0f) { // most used cuz HV outputs higher than 50A around 300A
        return highCurr;
    } else return lowCurr;
}

