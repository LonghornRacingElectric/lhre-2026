#pragma once
#include "drivers/lal/ILed.hpp"
#include "longhorn/led_base.h"

namespace lal {

class Stm32Led : public ILed {
public:
    explicit Stm32Led(const rainbow_led_t* config) : config_(config) {}
    void init() override { if (config_) led_init(config_); }
    void disable() override { led_disable(); }
    void set_color(float r, float g, float b) override { led_set(r, g, b); }
    void update_rainbow(float delta_time_s) override { led_rainbow(delta_time_s); }

private:
    const rainbow_led_t* config_;
};

} // namespace lal
