
#include <gtest/gtest.h>
#include "util/parameters/CurveParameter.h"
#include "util/parameters/SurfaceParameter.h"

TEST(Parameters, CurveParameter) {
    float arr[11] = {0.0f, 23.0f, 46.0f, 69.0f, 92.0f, 115.0f,
                     138.0f, 161.0f, 184.0f, 207.0f, 230.0f};
    CurveParameter c = CurveParameter(0.0f, 1.0f, arr);

    // within bounds, equal to existing points
    EXPECT_NEAR(c(0.0f), 0.0f, 0.001f);
    EXPECT_NEAR(c(0.5f), 115.0f, 0.001f);
    EXPECT_NEAR(c(1.0f), 230.0f, 0.001f);

    // within bounds, interpolation
    EXPECT_NEAR(c(0.07f), 16.1f, 0.001f);
    EXPECT_NEAR(c(0.57f), 131.1f, 0.001f);
    EXPECT_NEAR(c(0.97f), 223.1f, 0.001f);

    // out of bounds
    EXPECT_NEAR(c(-0.1f), 0.0f, 0.001f);
    EXPECT_NEAR(c(1.1f), 230.0f, 0.001f);
}

TEST(Parameters, SurfaceParameter) {
    float arr[11][11];
    for(int i = 0; i < 11; i++) {
        for(int j = 0; j < 11; j++) {
            arr[i][j] = (float)((j * j) + i);
        }
    }
    SurfaceParameter s = SurfaceParameter(-10.0f, 10.0f, -20.0f, 20.0f, arr);

    // within bounds, equal to existing points
    EXPECT_NEAR(s(10.0f, 20.0f), 110.0f, 0.001f);
    EXPECT_NEAR(s(-10.0f, 20.0f), 10.0f, 0.001f);
    EXPECT_NEAR(s(10.0f, -20.0f), 100.0f, 0.001f);
    EXPECT_NEAR(s(-10.0f, -20.0f), 0.0f, 0.001f);
    EXPECT_NEAR(s(8.0f, 16.0f), 90.0f, 0.001f);

    // within bounds, interpolation
    EXPECT_NEAR(s(7.9f, 5.2f), 86.45f, 0.001f);

    // out of bounds
    EXPECT_NEAR(s(-11.0f, -13.0f), 1.75f, 0.001f);
    EXPECT_NEAR(s(-3.0f, -21.0f), 12.5f, 0.001f);
    EXPECT_NEAR(s(11.0f, 21.0f), 110.0f, 0.001f);
}
