#include "shutdown_sense_sniff.h"

#include "longhorn/rtos/logger.h"
#include "main.h"

#include "cmsis_os.h"
#include <cmsis_os2.h>
#include <stdbool.h>

#define SHUTDOWN_SENSE_PRINT_PERIOD_MS 200U

static osThreadAttr_t shutdown_sense_thread_attrs = {
    .name = "SdwnSniff",
    .priority = osPriorityNormal,
    .stack_size = 512 * 8,
};

static void shutdown_sense_task(void *argument);
static bool read_pin(GPIO_TypeDef *port, uint16_t pin);

void dui_shutdown_sense_sniff_init(void) {
  // Start printing shutdown sense states.
  osThreadNew(shutdown_sense_task, NULL, &shutdown_sense_thread_attrs);
}

static void shutdown_sense_task(void *argument) {
  (void)argument;

  while (true) {
    // 1 means the shutdown sense input is OK.
    bool estop_ok =
        read_pin(SDSW_Sense_EStop_GPIO_Port, SDSW_Sense_EStop_Pin);
    bool inertia_ok = read_pin(SDWN_Sense_Inertia_SW_GPIO_Port,
                               SDWN_Sense_Inertia_SW_Pin);

    log_printf(LOG_INFO, "SDWN ESTOP:%u INERTIA:%u\n", (unsigned)estop_ok,
               (unsigned)inertia_ok);

    osDelay(pdMS_TO_TICKS(SHUTDOWN_SENSE_PRINT_PERIOD_MS));
  }
}

static bool read_pin(GPIO_TypeDef *port, uint16_t pin) {
  return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}
