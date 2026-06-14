#pragma once

namespace lal {

class ILed {
public:
    virtual ~ILed() = default;

    virtual void init() = 0;
    virtual void disable() = 0;

    // Sets the LED color based on RGB percentage (0.0 to 100.0)
    virtual void set_color(float r, float g, float b) = 0;

    // Runs a rainbow animation step based on delta time
    virtual void update_rainbow(float delta_time_s) = 0;
};

} // namespace lal
