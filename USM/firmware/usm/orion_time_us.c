#include "orion_time_us.h"
#include "main.h"
/* Local wrapping microsecond epoch. Call at least once per DWT wrap
 * (29 s at 144 MHz); periodic control/CAN calls normally run every few ms.
 * Clock changes and long suspension require restart and sync reacquisition. */
uint32_t orion_time_us(void) {
  static uint32_t previous, remainder, time_us;
  static uint8_t initialized;
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  if (!initialized) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    previous = DWT->CYCCNT;
    initialized = 1;
  }
  uint32_t current = DWT->CYCCNT;
  uint32_t divisor = SystemCoreClock / 1000000u;
  uint64_t cycles = (uint64_t)(uint32_t)(current - previous) + remainder;
  previous = current;
  if (divisor != 0u) {
    time_us += (uint32_t)(cycles / divisor);
    remainder = (uint32_t)(cycles % divisor);
  }
  __set_PRIMASK(mask);
  return time_us;
}
