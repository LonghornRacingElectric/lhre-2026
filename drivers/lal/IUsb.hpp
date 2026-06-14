#pragma once

#include <cstdarg>
#include <cstdint>

namespace lal {

class IUsb {
public:
    virtual ~IUsb() = default;

    virtual void init() = 0;

    // Prints a message with an appended newline
    virtual void println(const char* message) = 0;

    // Prints a formatted string
    virtual void printf(const char* format, ...) = 0;
    virtual void vprintf(const char* format, va_list args) = 0;

    // Raw transmission
    virtual uint8_t transmit(const uint8_t* buffer, uint16_t length) = 0;
};

} // namespace lal
