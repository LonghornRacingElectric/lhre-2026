#ifndef PDU_CAN_H
#define PDU_CAN_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void pdu_can_init(void);

// Read inverter feedback

/**
 * @brief Returns whether or not the vehicle is in park.
 *
 * @return true the vehicle is in park
 * @return false the vehicle is not in park
 */
bool vehicle_in_park(void);

/**
 * @brief Returns whether or not the vehicle is in drive.
 *
 * @return true the vehicle is in drive
 * @return false the vehicle is not in drive
 */
bool vehicle_in_drive(void);

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

#endif  // PDU_CAN_H