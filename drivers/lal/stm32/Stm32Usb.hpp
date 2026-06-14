#pragma once
#include "drivers/lal/IUsb.hpp"
#include "longhorn/usb_base.h"

namespace lal {

class Stm32Usb : public IUsb {
public:
    explicit Stm32Usb(CDC_Transmit_Fn_ptr transmit_fn) : transmit_fn_(transmit_fn) {}
    void init() override { if (transmit_fn_) usb_init(transmit_fn_); }
    void println(const char* message) override { usb_println(message); }
    void printf(const char* format, ...) override {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    void vprintf(const char* format, va_list args) override { v_usb_printf(format, args); }
    uint8_t transmit(const uint8_t* buffer, uint16_t length) override {
        if (transmit_fn_) return transmit_fn_(const_cast<uint8_t*>(buffer), length);
        return 1;
    }

private:
    CDC_Transmit_Fn_ptr transmit_fn_;
};

} // namespace lal
