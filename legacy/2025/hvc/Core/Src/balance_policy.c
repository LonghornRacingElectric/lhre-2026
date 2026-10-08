#include "balance_policy.h"

#define BALANCE_MIN_VOLTAGE_V 3.00f
#define BALANCE_MAX_VOLTAGE_V 4.20f
#define BALANCE_START_DELTA_V 0.010f
#define BALANCE_STOP_DELTA_V 0.005f

uint32_t balance_select_cells(const volatile float voltages[BALANCE_CELL_COUNT],
                              const bool monitored[BALANCE_CELL_COUNT],
                              const bool board_eligible[BALANCE_BMB_COUNT],
                              bool commands[BALANCE_CELL_COUNT]) {
    float minimum = BALANCE_MAX_VOLTAGE_V + 1.0f;
    bool have_minimum = false;

    for (uint32_t i = 0U; i < BALANCE_CELL_COUNT; ++i) {
        /* Even a board with no usable thermistor can contain the pack minimum. */
        if (!monitored[i]) continue;
        const float voltage = voltages[i];
        if (!(voltage >= BALANCE_MIN_VOLTAGE_V &&
              voltage <= BALANCE_MAX_VOLTAGE_V)) continue;
        if (voltage < minimum) minimum = voltage;
        have_minimum = true;
    }

    uint32_t count = 0U;
    for (uint32_t board = 0U; board < BALANCE_BMB_COUNT; ++board) {
        const uint32_t base = board * BALANCE_CELLS_PER_BMB;
        for (uint32_t cell = 0U; cell < BALANCE_CELLS_PER_BMB; ++cell) {
            const uint32_t index = base + cell;
            const float voltage = voltages[index];
            if (!have_minimum || !board_eligible[board] || !monitored[index] ||
                !(voltage >= BALANCE_MIN_VOLTAGE_V &&
                  voltage <= BALANCE_MAX_VOLTAGE_V)) {
                commands[index] = false;
            } else if (voltage > minimum + BALANCE_START_DELTA_V) {
                commands[index] = true;
            } else if (voltage < minimum + BALANCE_STOP_DELTA_V) {
                commands[index] = false;
            }
        }

        /* Highest-voltage cells get the limited thermal budget. */
        for (;;) {
            uint32_t board_count = 0U;
            uint32_t lowest_selected = BALANCE_CELL_COUNT;
            for (uint32_t cell = 0U; cell < BALANCE_CELLS_PER_BMB; ++cell) {
                const uint32_t index = base + cell;
                if (!commands[index]) continue;
                ++board_count;
                if (lowest_selected == BALANCE_CELL_COUNT ||
                    voltages[index] < voltages[lowest_selected]) {
                    lowest_selected = index;
                }
            }
            if (board_count <= BALANCE_MAX_CELLS_PER_BMB) {
                count += board_count;
                break;
            }
            commands[lowest_selected] = false;
        }
    }
    return count;
}
