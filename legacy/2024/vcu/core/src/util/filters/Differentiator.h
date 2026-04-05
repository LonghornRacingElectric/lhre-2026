#ifndef VCU_CORE_DIFFERENTIATOR_H
#define VCU_CORE_DIFFERENTIATOR_H

class Differentiator {
private:
    float last = 0.0f;
public:
    Differentiator();
    float get(float value, float deltaTime);
    void reset();
};


#endif //VCU_CORE_DIFFERENTIATOR_H
