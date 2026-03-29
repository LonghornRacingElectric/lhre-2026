#ifndef VCU_CORE_TRACKPOSITIONING_H
#define VCU_CORE_TRACKPOSITIONING_H


#include "VcuParameters.h"
#include "util/Structs.h"


typedef struct TrackPositioningInput {
    double gpsLat; // degrees
    double gpsLong; // degrees
    float gpsSpeed; // knots
    float gpsHeading; // degrees

    xyz imu1Accel; // accel x, y, z (m/s^2)
    xyz imu2Accel;
    xyz imu3Accel;
    xyz imu1Gyro; // gyro x, y, z (rad/s)
    xyz imu2Gyro;
    xyz imu3Gyro;

    float wheelDisplacementFl; // wheel speed (rad)
    float wheelDisplacementFr;
    float wheelDisplacementBl;
    float wheelDisplacementBr;

    float wheelAngleFl; // wheel angle (rad), left turn is +
    float wheelAngleFr;
    float wheelAngleBl;
    float wheelAngleBr;
} TrackPositioningInput;


typedef struct TrackPositioningOutput {
    xyz vehicleDisplacement;
    xyz vehicleVelocity;
    xyz vehicleAcceleration;
} TrackPositioningOutput;


class TrackPositioning {
public:
    void setParameters(VcuParameters* params) {};
    void evaluate(VcuParameters *params, TrackPositioningInput *input, TrackPositioningOutput *output, float deltaTime);
};


#endif //VCU_CORE_TRACKPOSITIONING_H
