#include "PRNDL.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>

void prndl_init(prndl_machine_t *machine) {
  machine->state = PRNDL_PARK;
  machine->prev_drive_switch = false;
  machine->drive_start_time_ms = 0;
}

void prndl_evaluate(prndl_machine_t *machine, const vcu_inputs_t *in,
                    vcu_outputs_t *out, uint32_t time_ms) {
  switch (machine->state) {
  case PRNDL_PARK:
    // Transition to DRIVE on (rising edge of switch) + brake + contactors
    if (rising_edge(machine->prev_drive_switch, in->drive_switch) &&
        in->contactors_closed && out->brake_active && out->pedal <= 1e-5) {
      machine->state = PRNDL_DRIVE;
      machine->drive_start_time_ms = time_ms;
    }
    break;
  case PRNDL_DRIVE:
    // Return to PARK if switch opened or contactors drop
    if (!in->drive_switch || !in->contactors_closed) {
      machine->state = PRNDL_PARK;
    }
    break;
  }

  machine->prev_drive_switch = in->drive_switch;
  out->prndl_state = machine->state;
}
