#pragma once
#include "drivers/lal/IDfu.hpp"
#include "longhorn/dfu_base.h"

namespace lal {

class Stm32Dfu : public IDfu {
public:
    explicit Stm32Dfu(dfu_config config) : config_(config) {}
    void init() override { dfu_init(config_); }
    void receive_data(const uint8_t* buffer, uint32_t length) override {
        dfu_receiveData(const_cast<uint8_t*>(buffer), length);
    }
    void check_status() override { check_dfu(); }
    void system_reset() override { if (config_.reset_fn) config_.reset_fn(); }

private:
    dfu_config config_;
};

} // namespace lal
