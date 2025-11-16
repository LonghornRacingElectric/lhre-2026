#ifndef DRIVERS_LONGHORN_LIB_RTOS_DFU_H
#define DRIVERS_LONGHORN_LIB_RTOS_DFU_H

#include "cmsis_os2.h"
#include "dfu_base.h"

/**
 * @brief Initializes DFU
 *
 */
void init_dfu(dfu_config config);

/**
 * @brief Starts a blocking DFU thread that only awakens once the USB
 interface
 * gets data
 *
 * @return osThreadId_t the thread id handle
 */
osThreadId_t dfu_start_thread();

#endif