#pragma once
#include "drivers/lal/IUsb.hpp"
#include <string>
#include <vector>
#include <cstdio>

namespace lal {

class MockUsb : public IUsb {
public:
    void init() override { initialized_ = true; }
    void println(const char* message) override {
        history_.push_back(std::string(message) + "\n");
    }
    void printf(const char* format, ...) override {
        char buf[512];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        if (len > 0) history_.push_back(std::string(buf, len));
    }
    void vprintf(const char* format, va_list args) override {
        char buf[512];
        int len = vsnprintf(buf, sizeof(buf), format, args);
        if (len > 0) history_.push_back(std::string(buf, len));
    }
    uint8_t transmit(const uint8_t* buffer, uint16_t length) override {
        transmitted_data_.insert(transmitted_data_.end(), buffer, buffer + length);
        return 0;
    }

    bool is_initialized() const { return initialized_; }
    const std::vector<std::string>& get_history() const { return history_; }
    const std::vector<uint8_t>& get_transmitted_data() const { return transmitted_data_; }
    void clear() { history_.clear(); transmitted_data_.clear(); }

private:
    bool initialized_ = false;
    std::vector<std::string> history_;
    std::vector<uint8_t> transmitted_data_;
};

} // namespace lal
