#include "argus_timer.h"
#include "stm32g4xx_hal.h"
#include "assert.h"
#include <stdint.h>
#include "tim.h"
// Forward declarations to avoid tim.h include conflict
extern TIM_HandleTypeDef htim6;
void MX_TIM6_Init(void);

// #include "usb_vcp.h"

/*! Callback function for PIT timer */
static timer_cb_t timer_callback_;
/*! Storage for the callback parameter */
static void *callback_param_;
/*! Timer interval in microseconds */
static uint32_t period_us_;

static uint32_t seconds;

/*!***************************************************************************
 * @brief Initializes the timer hardware.
 * @return -
 *****************************************************************************/
// void Timer_Init(void) {
// 	/* Initialize the timers, see generated main.c */
// 	// MX_TIM6_Init();
// 	// MX_TIM4_Init();
// 	// MX_TIM5_Init();
// 	/* Start the timers relevant for the LTC */
// 	//  HAL_TIM_Base_Start_IT(&htim2);
// 	__HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
//     __HAL_TIM_ENABLE(&htim2);
// 	// HAL_TIM_Base_Start(&htim5);
// 	seconds = 0;
// }
void Timer_Init(void) {
    MX_TIM6_Init();  // ensure TIM6 is initialized
    seconds = 0;
    // Configure TIM6 as free-running microsecond counter
    __HAL_TIM_SET_PRESCALER(&htim6, (SystemCoreClock / 1000000U) - 1);
    __HAL_TIM_SET_AUTORELOAD(&htim6, 0xFFFF);
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE(&htim6);
}
/*!***************************************************************************
 * @brief Obtains the lifetime counter value from the timers.
 *
 * @details The function is required to get the current time relative to any
 * point in time, e.g. the startup time. The returned values \p hct and
 * \p lct are given in seconds and microseconds respectively. The current
 * elapsed time since the reference time is then calculated from:
 *
 * t_now [µsec] = hct * 1000000 µsec + lct * 1 µsec
 *
 * @param hct A pointer to the high counter value bits representing current
 * time in seconds.
 * @param lct A pointer to the low counter value bits representing current
 * time in microseconds. Range: 0, .., 999999 µsec
 * @return -
 *****************************************************************************/
void Timer_GetCounterValue(uint32_t *hct, uint32_t *lct) {
    do {
        *lct = __HAL_TIM_GET_COUNTER(&htim6);
        *hct = seconds;
    } while (*lct > __HAL_TIM_GET_COUNTER(&htim6));
}


/*!***************************************************************************
 * @brief Installs an periodic timer callback function.
 * @details Installs an periodic timer callback function that is invoked whenever
 * an interval elapses. The callback is the same for any interval,
 * however, the single intervals can be identified by the passed
 * parameter.
 * Passing a zero-pointer removes and disables the callback.
 * @param f The timer callback function.
 * @return Returns the \link #status_t status\endlink (#STATUS_OK on success).
 *****************************************************************************/
status_t Timer_SetCallback(timer_cb_t f) {
	timer_callback_ = f;
	return STATUS_OK;
}

/**
 * @brief Period elapsed callback in non-blocking mode
 * @param htim TIM handle
 * @retval None
 */
// void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
// 	/* Trigger callback if the interrupt belongs to TIM4 and there is a callback */
// 	// if (htim == &htim6)
// 	// {
// 	// 	seconds += 1;
// 	// }
// 	// if (htim == &htim6 && timer_callback_) {
// 	// 	timer_callback_(callback_param_);
// 	// }

// 	if (htim == &htim6) {
//     	seconds += 1;
//     	if (timer_callback_) {
// 			timer_callback_(callback_param_);
// 		}
// 	}
// }

void Argus_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        seconds += 1;
        if (timer_callback_) { timer_callback_(callback_param_); }
    }
}

/*!***************************************************************************
 * @brief Starts the timer for a specified callback parameter.
 * @details Sets the callback interval for the specified parameter and starts
 * the timer with a new interval. If there is already an interval with
 * the given parameter, the timer is restarted with the given interval.
 * Passing an interval of 0 disables the timer.
 * @param dt_microseconds The callback interval in microseconds.
 * @param param An abstract parameter to be passed to the callback. This is
 * also the identifier of the given interval.
 * @return Returns the \link #status_t status\endlink (#STATUS_OK on success).
 *****************************************************************************/
status_t Timer_Start(uint32_t period, void *param) {
	callback_param_ = param;
	if (period == period_us_)
		return STATUS_OK;
	period_us_ = period;
	uint32_t prescaler = SystemCoreClock / 1000000U;
	while (period > 0xFFFF) {
		period >>= 1U;
		prescaler <<= 1U;
	}
	assert(prescaler <= 0x10000U);
	/* Set prescaler and period values */
	__HAL_TIM_SET_PRESCALER(&htim6, prescaler - 1);
	__HAL_TIM_SET_AUTORELOAD(&htim6, period - 1);
	/* Enable interrupt and timer */
	__HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
	__HAL_TIM_ENABLE(&htim6);
	return STATUS_OK;
}

/*!***************************************************************************
 * @brief Stops the timer for a specified callback parameter.
 * @details Stops a callback interval for the specified parameter.
 * @param param An abstract parameter that identifies the interval to be stopped.
 * @return Returns the \link #status_t status\endlink (#STATUS_OK on success).
 *****************************************************************************/
status_t Timer_Stop(void *param) {
	period_us_ = 0;
	callback_param_ = 0;
	/* Disable interrupt and timer */
	__HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
	__HAL_TIM_DISABLE(&htim6);;
	return STATUS_OK;
}

/*!***************************************************************************
 * @brief Sets the timer interval for a specified callback parameter.
 * @details Sets the callback interval for the specified parameter and starts
 * the timer with a new interval. If there is already an interval with
 * the given parameter, the timer is restarted with the given interval.
 * If the same time interval as already set is passed, nothing happens.
 * Passing a interval of 0 disables the timer.
 * @param dt_microseconds The callback interval in microseconds.
 * @param param An abstract parameter to be passed to the callback. This is
 * also the identifier of the given interval.
 * @return Returns the \link #status_t status\endlink (#STATUS_OK on success).
 *****************************************************************************/
status_t Timer_SetInterval(uint32_t dt_microseconds, void *param) {
	assert(dt_microseconds == 0 || dt_microseconds > 100);

	/* Disable interrupt and timer */
	callback_param_ = 0;
	HAL_TIM_Base_Stop_IT(&htim6);
	__HAL_TIM_CLEAR_IT(&htim6, TIM_IT_UPDATE);

	if (dt_microseconds)
	{
		/* Determine the prescaler and counter period values such that
		 * the period fits into 16-bits. */
		uint32_t prescaler = SystemCoreClock / 1000000U;
		uint32_t period = dt_microseconds;

		while (period > 0xFFFF)
		{
			period >>= 1U;
			prescaler <<= 1U;
		}

		assert(prescaler < 0x10000U);

		/* Set prescaler and period values and reset counter. */
		__HAL_TIM_SET_PRESCALER(&htim6, prescaler - 1);
		__HAL_TIM_SET_AUTORELOAD(&htim6, period - 1);
		__HAL_TIM_SET_COUNTER(&htim6, period - 1);

		/* The following generates an update event that triggers and update
		 * of the auto-reload into the internal shadow registers. This is
		 * required to update the timer configuration before the next update
		 * event (i.e. under/overflow). Unfortunately this also generates
		 * and immediate interrupt which is cleared in the next statement. */
		HAL_TIM_GenerateEvent(&htim6, TIM_EVENTSOURCE_UPDATE);
		__HAL_TIM_CLEAR_IT(&htim6, TIM_IT_UPDATE); // clear interrupt

		/* Enable interrupt and timer */
		callback_param_ = param;
		HAL_TIM_Base_Start_IT(&htim6);
	}

	return STATUS_OK;
}
