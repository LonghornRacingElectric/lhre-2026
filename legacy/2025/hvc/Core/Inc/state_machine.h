#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

/* Set while cells are unmonitored (a BMB removed or dead channels): the HVC
   then never enters a charging state and stays de-energized while a charger
   is connected. */
#ifndef HVC_DISABLE_CHARGING
#define HVC_DISABLE_CHARGING 0
#endif

typedef enum {
  HVC_STATE_NOT_ENERGIZED = 0,
  HVC_STATE_PRECHARGING = 1,
  HVC_STATE_ENERGIZED = 2,
  HVC_STATE_CHARGING_PRECHARGING = 3,
  HVC_STATE_CHARGING = 4,
} hvc_state_t;

/* Legacy aliases retained for the ADBMS balancing interface. */
#define STATE_NOT_ENERGIZED HVC_STATE_NOT_ENERGIZED
#define STATE_PRECHARGING HVC_STATE_PRECHARGING
#define STATE_ENERGIZED HVC_STATE_ENERGIZED
#define STATE_CHARGING HVC_STATE_CHARGING

void state_machine_init(void);
void update_state_machine(bool anyFaults);
hvc_state_t get_current_state(void);
const char *get_state_name(hvc_state_t state);
uint32_t get_precharge_qualified_ms(void);
bool isChargingBlocked(void);

#endif // STATE_MACHINE_H
