#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/AppsProcessor.h"
#include "blocks/Stompp.h"

//Check for two main requirements. Enable STOMPP IF:
//  Brakes are active
//  AND
//  Accelerator Travel > 25%

//Disable when
//  Accelerator Travel < 5%

TEST(Stompp, StomppRulesCompliance){
    Stompp stomppProcessor;
    StomppInput stomppInput = {};
    StomppOutput stomppOutput = {};

    VcuParameters vcuParameters = {};
    vcuParameters.stomppMechanicalBrakesThreshold = 0.1f;

    //Test 0% apps with no break pressure - Stompp remains disabled
    stomppInput = {0.0f, 0.0f};
    stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
    EXPECT_TRUE(stomppOutput.ok);
    stomppProcessor.reset();

    //Test 0%-100% accelerator travel with insignificant break pressure - Stompp remains disabled
    for(int appsInRange = 0; appsInRange < 100; appsInRange++) {
        stomppInput = {(float)(appsInRange)/100, 0.05f};
        stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
        EXPECT_TRUE(stomppOutput.ok);
        stomppProcessor.reset();
    }

    //Test 0% accelerator travel with full scale break pressure (0%-100%)
    for(int bseInRange = 10; bseInRange < 1000; bseInRange++) {
        stomppInput = {0.0f, (float)bseInRange};
        stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
        EXPECT_TRUE(stomppOutput.ok);
        stomppProcessor.reset();
    }

    //Test 26% accelerator travel with breaks applied
    stomppInput = {0.26f, 11.0f};
    stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
    EXPECT_FALSE(stomppOutput.ok);
    stomppProcessor.reset();

    //Test 0%-100% accelerator travel with breaks applied
    for(int appsInRange = 0; appsInRange < 100; appsInRange++) {
        stomppInput = {(float)appsInRange/100, 11.0f};
        stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
        if(((float)appsInRange/100) > 0.25f)
            EXPECT_FALSE(stomppOutput.ok);
        else if(((float)appsInRange/100) <= 0.25f)
            EXPECT_TRUE(stomppOutput.ok);
        stomppProcessor.reset();
    }

    //Test 26% accelerator travel with full scale break pressure
    for(int bseInRange = 0; bseInRange < 1000;  bseInRange++) {
        stomppInput = {0.26, (float)bseInRange};
        stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
        if((float)bseInRange > vcuParameters.stomppMechanicalBrakesThreshold)
            EXPECT_FALSE(stomppOutput.ok);
        else if((float)bseInRange <=vcuParameters.stomppMechanicalBrakesThreshold)
            EXPECT_TRUE(stomppOutput.ok);
        stomppProcessor.reset();
    }

    //Test if STOMPP disables when accelerator travel falls under 25 while brakes are applied
    stomppProcessor.toggleStompp();
    for(int appsInRange = 100; appsInRange > 0; appsInRange--) {
        stomppInput = {(float) appsInRange / 100, 11.0f};
        stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
        if (((float) appsInRange / 100) >= 0.05f)
            EXPECT_FALSE(stomppOutput.ok);
        else if (((float) appsInRange / 100) < 0.05f)
            EXPECT_TRUE(stomppOutput.ok);
    }
    stomppProcessor.reset();

    //Test if STOMPP disables when accelerator travel falls under 25 while brakes disabled
    stomppProcessor.toggleStompp();
    for(int appsInRange = 100; appsInRange > 0; appsInRange--) {
        stomppInput = {(float) appsInRange / 100, 0.0f};
        stomppProcessor.evaluate(&vcuParameters, &stomppInput, &stomppOutput, 1.0f);
        if (((float) appsInRange / 100) >= 0.05f)
            EXPECT_FALSE(stomppOutput.ok);
        else if (((float) appsInRange / 100) < 0.05f)
            EXPECT_TRUE(stomppOutput.ok);
    }
    stomppProcessor.reset();
}
