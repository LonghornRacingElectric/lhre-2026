#pragma once

#include <cstdint>

namespace lal {

class ISystemTimer {
public:
    virtual ~ISystemTimer() = default;
    virtual uint32_t get_tick_ms() const = 0;
    virtual void delay_ms(uint32_t delay) = 0;
};

} // namespace lal
