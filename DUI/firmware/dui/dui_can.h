#ifndef DUI_CAN_H
#define DUI_CAN_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t r2d_switch;
  uint8_t prndl_state;
  uint8_t ready_to_drive_buzzer;
  bool vcu_state_timed_out;
  bool buzzer_active;
} dui_can_debug_state_t;

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
 * @brief Returns whether the VCU is requesting the ready-to-drive buzzer.
 *
 * @return true if the buzzer should be active
 * @return false if the buzzer should be off
 */
bool dui_r2d_buzzer_active(void);

/**
 * @brief Returns DUI CAN debug state for one-line logging.
 */
dui_can_debug_state_t dui_can_get_debug_state(void);

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
