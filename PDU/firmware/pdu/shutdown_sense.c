#include "shutdown_sense.h"

#include "lv_battery.h"
#include "main.h"

static bool read_pin(GPIO_TypeDef *port, uint16_t pin) {
    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}

shutdown_sense_t shutdown_sense_read(void) {
    return (shutdown_sense_t){
        .leg_1 = lv_battery_alive(),
        .leg_11 = read_pin(SDWN_11_Sense_GPIO_Port, SDWN_11_Sense_Pin),
        .leg_12 = read_pin(SDWN_12_Sense_GPIO_Port, SDWN_12_Sense_Pin),
        .leg_13 = read_pin(SDWN_13_Sense_GPIO_Port, SDWN_13_Sense_Pin),
        .leg_14 = read_pin(SDWN_14_Sense_GPIO_Port, SDWN_14_Sense_Pin),
        .leg_15 = read_pin(SDWN_15_Sense_GPIO_Port, SDWN_15_Sense_Pin),
    };
}

bool shutdown_sense_closed(void) {
    shutdown_sense_t sense = shutdown_sense_read();

    return sense.leg_1 && sense.leg_11 && sense.leg_12 && sense.leg_13 &&
           sense.leg_14 && sense.leg_15;
}
