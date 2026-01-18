// dfu_base.h
// Methods for putting the MCU into a reset state
#ifndef LONGHORN_LIBRARY_2025_DFU_BASE_H
#define LONGHORN_LIBRARY_2025_DFU_BASE_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*SystemReset_fn)(void);
typedef void (*Delay_fn)(uint32_t delay);
typedef void (*PinSet_fn)(void* gpiox, uint16_t pin, uint8_t pin_state);
typedef long (*SemaphoreRelease_Fn)(void*, long* const);
typedef void (*Yield_Fn)(void);

typedef struct dfu_config {
    SystemReset_fn reset_fn;
    Delay_fn delay_fn;
    PinSet_fn pin_set_fn;
    SemaphoreRelease_Fn semaphore_release_fn;
    Yield_Fn yield_fn;
    void* semaphore_id;
    void* gpiox;
    uint16_t pin;
} dfu_config;

/**
 * @brief DFU initialization function.
 *
 * @param config Configuration struct with necessary callbacks and GPIO info.
 */
void dfu_init(dfu_config config);

/**
 * @brief  Receives data from the serial USB interface and writes it to the
 * circular buffer.
 * @note   This function is assumed to be called from an interrupt or a
 * high-priority callback context (like a USB driver).
 * @param  buf: Pointer to the new data that just arrived.
 * @param  len: The number of bytes in the 'buf'.
 */
void dfu_receiveData(uint8_t* buf, uint32_t len);

/**
 * @brief Periodic function to check DFU status, automatically invokes reset
 * function if command is received
 *
 */
void check_dfu();

#endif
