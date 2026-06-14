#pragma once
#include "drivers/lal/IDfu.hpp"
#include <vector>

namespace lal {

class MockDfu : public IDfu {
public:
    void init() override { initialized_ = true; }
    void receive_data(const uint8_t* buffer, uint32_t length) override {
        received_data_.insert(received_data_.end(), buffer, buffer + length);
    }
    void check_status() override { check_status_called_ = true; }
    void system_reset() override { reset_called_ = true; }

    bool is_initialized() const { return initialized_; }
    const std::vector<uint8_t>& get_received_data() const { return received_data_; }
    bool is_check_status_called() const { return check_status_called_; }
    bool is_reset_called() const { return reset_called_; }

private:
    bool initialized_ = false;
    std::vector<uint8_t> received_data_;
    bool check_status_called_ = false;
    bool reset_called_ = false;
};

} // namespace lal
