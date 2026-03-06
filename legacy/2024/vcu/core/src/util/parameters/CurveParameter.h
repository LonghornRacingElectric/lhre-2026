#ifndef VCU_CORE_CURVEPARAMETER_H
#define VCU_CORE_CURVEPARAMETER_H


#include <vector>


class CurveParameter {
private:
    float x0 = 0;
    float x1 = 0;
    float y[11]{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
public:
    CurveParameter() = default; // y = 1
    CurveParameter(float x0, float x1, float y[11]);

    CurveParameter(float xP, float yP); // y = kx
    float operator()(float x);
};


#endif //VCU_CORE_CURVEPARAMETER_H
