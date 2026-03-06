#include <cmath>
#include "Prndl.h"
#include "util/filters/Timer.h"

/**
 * PRNDL (pronounced "prindle") = Park, Reverse, Neutral, Drive, Low. At least that's what it is in a real car.
 * Our car only has Park and Drive and a single switch to decide which one we're in.
 *
 * Make sure the driver is pressing the brake before they try to go into drive. Also, if the car is in drive
 * when it turns on (someone flipped the drive/park switch while the car was off or something), do not start in drive.
 * Just ignore the fact that it's in drive until they properly switch from park to drive with the brake pressed.
 *
 * Also make sure the Ready to Drive sound is played through the buzzer when we successfully switch into drive.
 * The judges would be upset if the sound played even when the inverter isn't capable of spinning the motor (say
 * the HV system is inactive).
 *
 * Read the rules about park/drive and the Ready to Drive Sound!
 */

void Prndl::evaluate(VcuParameters *params, PrndlInput *input, PrndlOutput *output, float deltaTime) {
    switchInputDebounce.add(input->driveSwitch, deltaTime);
    bool driveSwitch = switchInputDebounce.get();
    bool driveSwitchRisingEdge = (driveSwitch > driveSwitchLastState);
    driveSwitchLastState = driveSwitch;

    if (state) {
        // we're currently in Drive
        buzzerTimer.count(deltaTime);

        if (!driveSwitch || !input->inverterReady) {
            state = false;
        }

    } else {
        // we're currently in Park
        bool brakesPressed = (input->bse > params->brakesSoftThreshold);
        bool acceleratorReleased = (input->apps == 0.0f);

        if (driveSwitchRisingEdge && input->inverterReady && brakesPressed && acceleratorReleased) {
            state = true; // switch to Drive
            buzzerTimer.reset();
        }
    }

    bool pattern = fmod(params->prndlBuzzerDuration - buzzerTimer.time, 0.25f) < 0.20f;

    output->state = state; // Park = false, Drive = true
    output->buzzer = state && (!buzzerTimer.isFinished()) && pattern; // buzz if we're in the first X seconds of Drive
}

void Prndl::setParameters(VcuParameters *params) {
    buzzerTimer = Timer(params->prndlBuzzerDuration);
    switchInputDebounce = Debounce(params->prndlSwitchDebounceDuration, switchInputDebounce.get());
}
