#include "drivers/lal/stm32/Stm32Can.hpp"
#include "drivers/lal/stm32/Stm32SystemTimer.hpp"
#include "drivers/lal/stm32/Stm32Led.hpp"
#include "drivers/lal/stm32/Stm32Usb.hpp"
#include "drivers/lal/stm32/Stm32Dfu.hpp"

void verify_lal_compilation() {
    // Basic verification of constructor and method calls
    lal::Stm32SystemTimer timer;
    timer.get_tick_ms();

    rainbow_led_t led_cfg = {};
    lal::Stm32Led led(&led_cfg);

    lal::Stm32Usb usb(nullptr);

    dfu_config dfu_cfg = {};
    lal::Stm32Dfu dfu(dfu_cfg);

    lal::Stm32Can can(nullptr);
}
