#ifndef DUI_R2D_H
#define DUI_R2D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"

/**
 * @brief Starts a thread that checks ready to drive status and updates CAN
 */
void dui_r2d_init(void);

/**
 * @brief Checks if the ready to drive is enabled
 * @retval True if the ready to drive is enabled, false otherwise
 */
bool is_r2d_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // DUI_R2D_H