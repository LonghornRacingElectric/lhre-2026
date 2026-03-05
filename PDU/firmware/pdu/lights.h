#ifndef PDU_LIGHTS_H
#define PDU_LIGHTS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "tim.h"

void lights_init(void);

void setPWM(TIM_HandleTypeDef* htim, uint32_t channel, float percentage);

#ifdef __cplusplus
}
#endif
#endif  // PDU_LIGHTS_H