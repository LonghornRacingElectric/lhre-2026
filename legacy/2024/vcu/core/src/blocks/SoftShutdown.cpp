#include "SoftShutdown.h"

/**
 * Software-level shutdown of the motor. Kind of like the shutdown circuit but for the code.
 * Happens if any critical sensor reading is out of bounds or the driver presses both pedals for example.
 */

void SoftShutdown::evaluate(VcuParameters *params, SoftShutdownInput *input, SoftShutdownOutput *output,
                            float deltaTime) {

    bool everythingOk = (input->appsOk && input->bseOk && input->stomppOk && input->prndlState);

    output->enableInverter = everythingOk;
    output->inverterTorqueRequest = (output->enableInverter) ? input->torqueRequest : 0;

}
