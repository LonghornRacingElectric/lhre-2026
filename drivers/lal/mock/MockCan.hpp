#pragma once
#include "drivers/lal/ICan.hpp"
#include <vector>

namespace lal {

class MockCan : public ICan {
public:
    bool init() override { initialized_ = true; return true; }
    bool start() override { started_ = true; return true; }
    bool stop() override { started_ = false; return true; }
    bool send(const CanMessage& msg) override {
        sent_messages_.push_back(msg);
        return true;
    }
    void register_rx_callback(RxCallback callback, void* context) override {
        rx_callback_ = callback;
        rx_context_ = context;
    }
    uint32_t get_tx_fifo_free_level() const override { return 3; }

    bool is_initialized() const { return initialized_; }
    bool is_started() const { return started_; }
    const std::vector<CanMessage>& get_sent_messages() const { return sent_messages_; }
    void simulate_rx(const CanMessage& msg) {
        if (rx_callback_) rx_callback_(msg, rx_context_);
    }
    void clear() { sent_messages_.clear(); }

private:
    bool initialized_ = false;
    bool started_ = false;
    std::vector<CanMessage> sent_messages_;
    RxCallback rx_callback_ = nullptr;
    void* rx_context_ = nullptr;
};

} // namespace lal
