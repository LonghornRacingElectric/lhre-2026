#ifndef BALANCE_POLICY_H
#define BALANCE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#define BALANCE_BMB_COUNT 10U
#define BALANCE_CELLS_PER_BMB 14U
#define BALANCE_CELL_COUNT (BALANCE_BMB_COUNT * BALANCE_CELLS_PER_BMB)
#define BALANCE_MAX_CELLS_PER_BMB 4U

/* Pure selection logic; hardware and safety gating are handled by cells.c. */
uint32_t balance_select_cells(const volatile float voltages[BALANCE_CELL_COUNT],
                              const bool monitored[BALANCE_CELL_COUNT],
                              const bool board_eligible[BALANCE_BMB_COUNT],
                              bool commands[BALANCE_CELL_COUNT]);

#endif
