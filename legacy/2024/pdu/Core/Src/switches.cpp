#include "switches.h"

#include "tim.h"

void switches_init() {
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
}

void switches_setBrakeLight(float f) {
  TIM1->CCR3 = (int)(f * 10000);
}

void switches_setPump(float f) {

}

void switches_setAccessory(float f) {

}

void switches_setRadiatorFans(float f) {

}

void switches_setBatteryFans(bool on) {

}

void switches_setGLV(bool on) {

}

void switches_setShutdown(bool on) {

}
