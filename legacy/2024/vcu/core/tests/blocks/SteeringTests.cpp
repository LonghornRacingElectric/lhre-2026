
#include <gtest/gtest.h>
#include "../../src/blocks/Steering.h"
#include "../../src/VcuParameters.h"

TEST(Steering, Steering) {
    Steering steering;
    SteeringInput steeringInput = {};
    SteeringOutput steeringOutput = {};

    VcuParameters params = {};

    //WAIT MAYBE FIX ARRAY VALUES INTO DECIMAL?????
    //INNER WHEEL ARRAY//
    float arrI[11] = {0.044f, 0.0398f, 0.0792f, 0.1185f, 0.1576f, 0.1976f, 0.2356f, 0.2745f, 0.3135f, 0.3526f, 0.392f};
    params.steeringWheelToInnerWheel = CurveParameter(0.0f, 90.0f, arrI);

    //OUTER WHEEL ARRAY//
    float arrO[11] = {0.0044f, 0.0447f, 0.0855f, 0.1267f, 0.1688f, 0.2115f, 0.2553f, 0.2953f, 0.3363f, 0.3838f, 0.4388f};
    params.steeringWheelToOuterWheel = CurveParameter(0.0f, 90.0f, arrO);

    params.steeringPotMaxVoltage = 3.3f;
    params.steeringWheelMaxAngle = 90.0f;

    //TEST -- VOLTAGE -> STEERING ANGLE
    // Test voltage to steering angle for 0
    steeringInput = {1.65f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.steeringWheelAngle, 0.0f);
    // Test voltage to steering angle for -90
    steeringInput = {0.0f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.steeringWheelAngle, 90.0f);
    // Test voltage to steering angle for 90
    steeringInput = {3.3f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.steeringWheelAngle, -90.0f);

    //TEST -- STEERING ANGLE -> WHEEL ANGLES
    //Test steering angle to FR wheel angle for 0 (1.65V)
    steeringInput = {1.65f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.wheelAngleFr, 0.0f);
    //Test steering angle to FR wheel angle for -90 (3.3V)
    steeringInput = {3.3f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.wheelAngleFr, -0.3920);
    //Test steering angle to FL wheel angle for -90 (3.3V)
    steeringInput = {3.3f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.wheelAngleFl, -0.4388);

    //TEST -- STEERING ANGLE -> BACK ANGLES
    steeringInput = {0.0f};
    steering.evaluate(&params, &steeringInput, &steeringOutput, 1.0f);
    EXPECT_FLOAT_EQ(steeringOutput.wheelAngleBr, 0.0f);

}



//edit red stuff to steering