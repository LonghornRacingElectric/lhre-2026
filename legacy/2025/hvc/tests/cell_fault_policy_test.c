#include "cell_fault_policy.h"

#include <assert.h>
#include <stdbool.h>

int main(void)
{
    assert(!cell_fault_is_high_impedance_suspect(true, 3.40f));
    assert(cell_fault_is_high_impedance_suspect(true, 0.25f));
    assert(cell_fault_is_high_impedance_suspect(true, -0.70f));
    assert(cell_fault_is_high_impedance_suspect(true, 5.20f));
    assert(!cell_fault_is_high_impedance_suspect(true, 4.30f));
    assert(!cell_fault_is_high_impedance_suspect(false, 0.25f));

    assert(!cell_fault_is_undervoltage(true, 3.40f, 3.00f, true));
    assert(!cell_fault_is_undervoltage(true, 0.25f, 3.00f, true));
    assert(cell_fault_is_undervoltage(true, 0.25f, 3.00f, false));

    /* Plausible low cells remain safety faults even with the bypass enabled. */
    assert(cell_fault_is_undervoltage(true, 2.80f, 3.00f, true));
    assert(cell_fault_is_undervoltage(false, 0.25f, 3.00f, true));

    /* Realistic OV remains a fault; only an implausibly high reading bypasses. */
    assert(cell_fault_is_overvoltage(true, 4.30f, 4.20f, true));
    assert(!cell_fault_is_overvoltage(true, 5.20f, 4.20f, true));
    assert(cell_fault_is_overvoltage(true, 5.20f, 4.20f, false));
    assert(cell_fault_is_overvoltage(false, 5.20f, 4.20f, true));
    return 0;
}
