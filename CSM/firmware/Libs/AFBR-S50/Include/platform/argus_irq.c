//
// Created by Kaitlyn on 2/24/2025. Modified by Arav Karnik on 4/1/2026
//

#include "stm32g4xx.h" //changed to match the STM32G474xx MCU
#include "argus_irq.h"
#include <assert.h>

// static global counter to keep track of nested IRQ locks
static volatile uint32_t g_irq_lock_ct = 0;

/*!***************************************************************************
 * @brief   Enable IRQ Interrupts
 *
 * @details Enables IRQ interrupts and enters an atomic or critical section.
 *
 * @note    The IRQ_LOCK might get called multiple times. Therefore, the
 *          API expects that the IRQ_UNLOCK must be called as many times as
 *          the IRQ_LOCK was called before the interrupts are enabled.
 *          Global unlock all interrupts using CMSIS function "__enable_irq()".
 *****************************************************************************/

void IRQ_UNLOCK(void) {
	assert(g_irq_lock_ct > 0);
	if (--g_irq_lock_ct <= 0) {
		g_irq_lock_ct = 0;
		__enable_irq();
	}
}


/*!***************************************************************************
 * @brief   Disable IRQ Interrupts
 *
 * @details Disables IRQ interrupts and leaves the atomic or critical section.
 *
 * @note    The IRQ_LOCK might get called multiple times. Therefore, the
 *          API expects that the IRQ_UNLOCK must be called as many times as
 *          the IRQ_LOCK was called before the interrupts are enabled.
 *          Assumed to be called from a thread (non-interrupt) context.
 *****************************************************************************/

void IRQ_LOCK(void)
{
	__disable_irq();
	g_irq_lock_ct++;
}