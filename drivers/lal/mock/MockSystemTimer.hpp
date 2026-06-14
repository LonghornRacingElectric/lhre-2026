#pragma once
#include "drivers/lal/ISystemTimer.hpp"

namespace lal {

class MockSystemTimer : public ISystemTimer {
public:
    uint32_t get_tick_ms() const override { return tick_ms_; }
    void delay_ms(uint32_t delay) override { tick_ms_ += delay; }

    void advance_time_ms(uint32_t ms) { tick_ms_ += ms; }
    void set_time_ms(uint32_t ms) { tick_ms_ = ms; }

private:
    uint32_t tick_ms_ = 0;
};

} // namespace lal
