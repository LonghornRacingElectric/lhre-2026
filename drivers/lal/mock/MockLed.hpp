#pragma once
#include "drivers/lal/ILed.hpp"

namespace lal {

class MockLed : public ILed {
public:
    void init() override { initialized_ = true; }
    void disable() override { disabled_ = true; }
    void set_color(float r, float g, float b) override {
        r_ = r; g_ = g; b_ = b;
    }
    void update_rainbow(float delta_time_s) override {
        rainbow_updated_count_++;
        last_delta_time_s_ = delta_time_s;
    }

    bool is_initialized() const { return initialized_; }
    bool is_disabled() const { return disabled_; }
    float get_r() const { return r_; }
    float get_g() const { return g_; }
    float get_b() const { return b_; }
    int get_rainbow_updated_count() const { return rainbow_updated_count_; }
    float get_last_delta_time_s() const { return last_delta_time_s_; }

private:
    bool initialized_ = false;
    bool disabled_ = false;
    float r_ = 0.0f;
    float g_ = 0.0f;
    float b_ = 0.0f;
    int rainbow_updated_count_ = 0;
    float last_delta_time_s_ = 0.0f;
};

} // namespace lal
