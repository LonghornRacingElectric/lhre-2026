#include "cell_fault_policy.h"

#include <assert.h>
#include <stdbool.h>

int main(void)
{
    assert(cell_fault_is_monitored(12U));
    assert(!cell_fault_is_monitored(13U));
    assert(!cell_fault_is_monitored(14U));
    assert(!cell_fault_is_monitored(15U));
    assert(!cell_fault_is_monitored(16U));
    assert(cell_fault_is_monitored(17U));

    uint32_t monitoredCellCount = 0U;
    for (uint32_t cell = 0U; cell < 140U; cell++) {
        if (cell_fault_is_monitored(cell)) monitoredCellCount++;
    }
    assert(monitoredCellCount == 136U);

    assert(thermistor_reading_is_valid(23.5f));
    assert(thermistor_reading_is_valid(60.0f));
    assert(thermistor_reading_is_valid(135.0f));
    assert(!thermistor_reading_is_valid(-0.1f));
    assert(!thermistor_reading_is_valid(-22.0f));
    assert(!thermistor_reading_is_valid(-999.0f));
    assert(!thermistor_reading_is_valid(136.0f));
    assert(!thermistor_reading_is_overtemperature(-999.0f, 60.0f));
    assert(!thermistor_reading_is_overtemperature(60.0f, 60.0f));
    assert(thermistor_reading_is_overtemperature(60.1f, 60.0f));

    return 0;
}
