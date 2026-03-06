#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/TorqueMap.h"

TEST(TorqueMap, PowerLimit) {
    VcuParameters params;
    params.mapPedalToTorqueRequest = CurveParameter(1.0f, 230.0f);
    params.mapDerateBatterySoc = CurveParameter();
    params.mapDerateInverterTemp = CurveParameter();
    params.mapDerateInverterTemp = CurveParameter();
    params.mapDerateMotorTemp = CurveParameter();
    params.mapPowerLimit = 80000;
    params.mapPowerLimitFeedbackP = 0.01f;
    params.mapPowerLimitFeedbackTimeConstant = 0;

    TorqueMapInput torqueMapInput;
    TorqueMapOutput torqueMapOutput;
    TorqueMap torqueMap;

    torqueMap.setParameters(&params);

    torqueMapInput = {
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            500.0f,
            0.0f,
    };
    torqueMap.evaluate(&params, &torqueMapInput, &torqueMapOutput, 0.001f);
    EXPECT_FLOAT_EQ(torqueMapOutput.torqueRequest, 230.0f);

    torqueMapInput = {
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            500.0f,
            1000.0f,
    };
    torqueMap.evaluate(&params, &torqueMapInput, &torqueMapOutput, 0.001f);
    EXPECT_FLOAT_EQ(torqueMapOutput.torqueRequest, 0.0f);

    torqueMapInput = {
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            500.0f,
            200.0f,
    };
    torqueMap.evaluate(&params, &torqueMapInput, &torqueMapOutput, 0.001f);
    EXPECT_FLOAT_EQ(torqueMapOutput.torqueRequest, 30.0f);
}