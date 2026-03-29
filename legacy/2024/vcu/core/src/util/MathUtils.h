#include <math.h>

#ifndef VCU_CORE_MATHUTILS_H
#define VCU_CORE_MATHUTILS_H

static constexpr float map(const float value, const float fromLow, const float fromHigh, 
                            const float toLow, const float toHigh) {
    return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

#endif