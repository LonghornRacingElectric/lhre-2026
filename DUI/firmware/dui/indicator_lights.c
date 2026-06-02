#include "indicator_lights.h"

#include "dui_can.h"
#include "main.h"

#include "cmsis_os.h"
#include <cmsis_os2.h>

#define INDICATOR_LIGHTS_PERIOD_MS 10U

// Thread settings for the light update task.
static osThreadAttr_t indicator_lights_thread_attrs = {
    .name = "IndicatorLights",
    .priority = osPriorityNormal,
    .stack_size = 128 * 8,
};

static void indicator_lights_task(void *argument);
static void set_light(GPIO_TypeDef *port, uint16_t pin, bool on);

void dui_indicator_lights_init(void) {
  // Start the task that keeps the lights matched to the CAN packet.
  osThreadNew(indicator_lights_task, NULL, &indicator_lights_thread_attrs);
}

static void indicator_lights_task(void *argument) {
  (void)argument;

  while (true) {
    // Turn each light on when its error bit is set in packet 0x134.
    set_light(BMS_Error_SW_GPIO_Port, BMS_Error_SW_Pin, hvc_bms_fault());
    set_light(IMD_Error_SW_GPIO_Port, IMD_Error_SW_Pin, hvc_imd_fault());

    // Check often, but do not busy-wait.
    osDelay(pdMS_TO_TICKS(INDICATOR_LIGHTS_PERIOD_MS));
  }
}

static void set_light(GPIO_TypeDef *port, uint16_t pin, bool on) {
  // GPIO high means light on.
  HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
