
#include "TrackPositioning.h"

/**
 * Dead reckoning! Use accelerometer, GPS, and potentially even wheel speed and angle data to estimate the position
 * of the car within the track. Probably use an Extended Kalman Filter, and start with just accel and GPS.
 */
void TrackPositioning::evaluate(VcuParameters *params, TrackPositioningInput *input, TrackPositioningOutput *output,
                                float deltaTime) {
    // TODO implement
    output->vehicleDisplacement = {0, 0, 0}; // m
    output->vehicleVelocity = {0, 0, 0}; // m/s
    output->vehicleAcceleration = {0, 0, 0}; // m/s^2
}
