#include "imd_mode_policy.h"

#include <assert.h>

int main(void)
{
    imd_mode_policy_t policy;
    imd_mode_policy_init(&policy, 0U);

    /* AIRs open: the IMD reads fault by design and is ignored. */
    assert(imd_mode_policy_update(&policy, false, false, 0U) == IMD_STATE_DISARMED);
    assert(imd_mode_policy_update(&policy, false, false, 60000U) == IMD_STATE_DISARMED);
    assert(!imd_mode_policy_tripped(&policy));

    /* AIRs close: wait for OK; still faulting is fine inside the timeout. */
    assert(imd_mode_policy_update(&policy, true, false, 70000U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, false, 80000U) == IMD_STATE_WAIT_OK);

    /* OK must be held for the qualify time; a dropout restarts it. */
    assert(imd_mode_policy_update(&policy, true, true, 81000U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, false, 81200U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, true, 81300U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, true, 81799U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, true, 81800U) == IMD_STATE_ARMED);

    /* Armed: a fault shorter than the debounce does not trip. */
    assert(imd_mode_policy_update(&policy, true, false, 90000U) == IMD_STATE_ARMED);
    assert(imd_mode_policy_update(&policy, true, false, 90099U) == IMD_STATE_ARMED);
    assert(imd_mode_policy_update(&policy, true, true, 90100U) == IMD_STATE_ARMED);

    /* Key-off: disarm before the IMD sees 0 V, no trip. */
    assert(imd_mode_policy_update(&policy, false, true, 95000U) == IMD_STATE_DISARMED);
    assert(imd_mode_policy_update(&policy, false, false, 96000U) == IMD_STATE_DISARMED);
    assert(!imd_mode_policy_tripped(&policy));

    /* Re-energize and arm again. */
    assert(imd_mode_policy_update(&policy, true, true, 100000U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, true, 100500U) == IMD_STATE_ARMED);

    /* Sustained fault while armed: trip, and stay tripped whatever happens. */
    assert(imd_mode_policy_update(&policy, true, false, 110000U) == IMD_STATE_ARMED);
    assert(imd_mode_policy_update(&policy, true, false, 110100U) == IMD_STATE_TRIPPED);
    assert(policy.tripReason == IMD_TRIP_FAULT);
    assert(imd_mode_policy_update(&policy, false, true, 120000U) == IMD_STATE_TRIPPED);
    assert(imd_mode_policy_update(&policy, true, true, 200000U) == IMD_STATE_TRIPPED);

    /* IMD never reads OK after the AIRs close: trip on the timeout. */
    imd_mode_policy_init(&policy, 0U);
    assert(imd_mode_policy_update(&policy, true, false, 1000U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, false, 30999U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, false, 31000U) == IMD_STATE_TRIPPED);
    assert(policy.tripReason == IMD_TRIP_TIMEOUT);

    /* Tick wraparound inside the qualify window. */
    imd_mode_policy_init(&policy, 0xFFFFFF00U);
    assert(imd_mode_policy_update(&policy, true, true, 0xFFFFFF00U) == IMD_STATE_WAIT_OK);
    assert(imd_mode_policy_update(&policy, true, true, 0x000000F4U) == IMD_STATE_ARMED);
    return 0;
}
