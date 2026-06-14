#pragma once
#include "drivers/lal/ISystemTimer.hpp"
#include "stm32g4xx_hal.h"

namespace lal {

class Stm32SystemTimer : public ISystemTimer {
public:
    uint32_t get_tick_ms() const override { return HAL_GetTick(); }
    void delay_ms(uint32_t delay) override { HAL_Delay(delay); }
};

} // namespace lal
