#pragma once

#include <cstddef>
#include <cstdint>

namespace lal {

class IDfu {
public:
    virtual ~IDfu() = default;

    virtual void init() = 0;
    virtual void receive_data(const uint8_t* buffer, uint32_t length) = 0;
    virtual void check_status() = 0;
    virtual void system_reset() = 0;
};

} // namespace lal
