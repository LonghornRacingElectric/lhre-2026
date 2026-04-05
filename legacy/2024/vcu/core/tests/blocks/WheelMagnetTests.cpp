
#include <gtest/gtest.h>
#include "VcuModel.h"

TEST(WheelMagnets, Wheel) {
    VcuParameters params;
    WheelMagnets wheelMagnets;

//    params.wheelMagnetLpfTimeConstant = 0.010f;
    params.wheelMagnetLpfTimeConstant = 0.030f;


    wheelMagnets.setParameters(&params);
    WheelMagnetsOutput output = {0,0,0,0};
    WheelMagnetsInput input = {0,0,0,0, true};
    float trueAngularVelocity = 100.0f; // rads per second of wheel
    float dt = 0.003f;
    float amplitude = 15.0f;
    for(int i=0; i < 1000; i++){
            auto wave = amplitude * sin(3.0f*trueAngularVelocity*i*dt);
            input.wheelMagneticFieldFr = wave;
            input.wheelMagneticFieldFl = wave;
            input.wheelMagneticFieldBl = wave;
            input.wheelMagneticFieldFr = wave;
            wheelMagnets.evaluate(&params, &input, &output, dt);
    }
    EXPECT_NEAR(output.wheelSpeedFl, trueAngularVelocity, 3.0f); // 3% error


}