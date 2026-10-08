#include "balance_policy.h"

#include <assert.h>

static volatile float voltages[BALANCE_CELL_COUNT];
static bool monitored[BALANCE_CELL_COUNT];
static bool board_eligible[BALANCE_BMB_COUNT];
static bool commands[BALANCE_CELL_COUNT];

static void reset(void) {
    for (uint32_t i = 0U; i < BALANCE_CELL_COUNT; ++i) {
        voltages[i] = 3.500f;
        monitored[i] = true;
        commands[i] = false;
    }
    for (uint32_t i = 0U; i < BALANCE_BMB_COUNT; ++i) {
        board_eligible[i] = true;
    }
}

int main(void) {
    reset();
    voltages[0] = 3.480f;
    for (uint32_t i = 1U; i < BALANCE_CELL_COUNT; ++i) {
        voltages[i] = 3.500f + (float)i * 0.001f;
    }
    assert(balance_select_cells(voltages, monitored, board_eligible,
                                commands) == 40U);
    assert(!commands[0]);
    assert(!commands[1]);
    assert(commands[13]);
    assert(commands[139]);

    reset();
    voltages[0] = 3.500f;
    voltages[1] = 3.509f;
    voltages[2] = 3.511f;
    assert(balance_select_cells(voltages, monitored, board_eligible,
                                commands) == 1U);
    assert(!commands[1] && commands[2]);
    voltages[2] = 3.507f;
    assert(balance_select_cells(voltages, monitored, board_eligible,
                                commands) == 1U);
    voltages[2] = 3.504f;
    assert(balance_select_cells(voltages, monitored, board_eligible,
                                commands) == 0U);

    reset();
    voltages[13] = 0.5f;
    monitored[13] = false; /* Known dead C14 may never be bled. */
    voltages[0] = 3.48f;
    voltages[14] = 3.52f;
    board_eligible[1] = false;
    (void)balance_select_cells(voltages, monitored, board_eligible, commands);
    assert(!commands[13] && !commands[14]);

    reset();
    voltages[13] = 2.99f; /* A real UV cell is not a balance target. */
    voltages[14] = 4.21f; /* A real OV cell is not a balance target. */
    (void)balance_select_cells(voltages, monitored, board_eligible, commands);
    assert(!commands[13] && !commands[14]);
    return 0;
}
