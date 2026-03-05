#ifndef VCU_CORE_DRS_H
#define VCU_CORE_DRS_H


#include "VcuParameters.h"
#include "util/Structs.h"


typedef struct DragReductionSystemInput {
    float steeringWheelAngle;
    xyz vehicleVelocity;
} DragReductionSystemInput;


typedef struct DragReductionSystemOutput {
    bool enable;
} DragReductionSystemOutput;


class DragReductionSystem {
public:
    void setParameters(VcuParameters* params) {};
    void evaluate(VcuParameters *params, DragReductionSystemInput *input, DragReductionSystemOutput *output, float deltaTime);
};


#endif //VCU_CORE_DRS_H
