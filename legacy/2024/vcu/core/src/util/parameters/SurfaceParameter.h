#ifndef VCU_CORE_SURFACEPARAMETER_H
#define VCU_CORE_SURFACEPARAMETER_H

class SurfaceParameter {
private:
    float x0 = 0;
    float x1 = 0;
    float y0 = 0;
    float y1 = 0;
    float z[11][11] {};
public:
    SurfaceParameter() = default;
    SurfaceParameter(float x0, float x1, float y0, float y1, float z[11][11]);
    float operator()(float x, float y);
};


#endif //VCU_CORE_SURFACEPARAMETER_H
