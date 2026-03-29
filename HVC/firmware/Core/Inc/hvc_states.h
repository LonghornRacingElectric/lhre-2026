/* Exported types ------------------------------------------------------------*/

#ifndef HVC_STATES_H
#define HVC_STATES_H

/**
 * @brief HVC State Machine States
 */
typedef enum {
  HVC_STATE_NOT_ENERGIZED = 0,    /**< System powered down, contactors open */
  HVC_STATE_PRECHARGING = 1,          /**< Precharge in progress */
  HVC_STATE_ENERGIZED = 2,            /**< System powered, ready to drive */
  HVC_STATE_CHARGING_PRECHARGING = 3, /**< Charging mode precharge */
  HVC_STATE_CHARGING = 4              /**< Charging mode active */
} hvc_state_t;

#endif /* HVC_STATES_H */