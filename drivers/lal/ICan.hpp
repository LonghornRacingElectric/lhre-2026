#pragma once

#include <cstddef>
#include <cstdint>

namespace lal {

struct CanMessage {
    uint32_t id;
    bool is_extended;
    uint8_t dlc;
    uint8_t data[64];
};

class ICan {
public:
    virtual ~ICan() = default;

    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;

    // Sends a single message to the bus
    virtual bool send(const CanMessage& msg) = 0;

    // A callback type for receiving messages
    using RxCallback = void (*)(const CanMessage& msg, void* context);

    // Register a callback for incoming messages
    virtual void register_rx_callback(RxCallback callback, void* context) = 0;

    // Returns the number of available slots in the TX FIFO
    virtual uint32_t get_tx_fifo_free_level() const = 0;
};

} // namespace lal
