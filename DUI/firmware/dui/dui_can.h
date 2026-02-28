#ifndef DUI_CAN_H
#define DUI_CAN_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void dui_can_init(void);

/**
 * @brief Sets Ready to Drive Status in the out CAN mailbox
 *
 * @param r2d true if the vehicle is ready to drive, false if not
 */
void dui_set_r2d(bool r2d);

/**
 * @brief Returns whether or not the HVC has an IMD fault from the HVC
 *
 * @return true if there is an IMD fault
 * @return false if there is no IMD fault
 */
bool hvc_imd_fault(void);

/**
 * @brief Returns whether or not the HVC has a BMS fault from the HVC
 *
 * @return true if there is a BMS fault
 * @return false if there is no BMS fault
 */
bool hvc_bms_fault(void);

#ifdef __cplusplus
}
#endif

#endif // DUI_CAN_H
