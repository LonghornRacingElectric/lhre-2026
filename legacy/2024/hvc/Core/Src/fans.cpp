
#include "fans.h"
#include "tim.h"
#include "main.h"
#include "LonghornLib/angel_can.h"
#include "LonghornLib/clock.h"

static CanInbox coolingInbox;

/**
 * Ramps fan PWM to requested PWM value from the VCU
 */
void setFanPwm(float pwm, float deltaTime) {
  pwmTimer += deltaTime;
  if (pwmTimer >= 0.02f) {
    pwmTimer = 0.0f;

    pwmDutyCycleMain += (pwmDutyCycleMain < (pwm / 100.0f)) ? 0.0001f : -0.0001f;
    if (pwmDutyCycleMain > 1.0f) pwmDutyCycleMain = 1.0f;
    if (pwmDutyCycleMain < 0.0f) pwmDutyCycleMain = 0.0f;

    pwmDutyCycleUnique += (pwmDutyCycleUnique < (pwm / 100.0f)) ? 0.0001f : -0.0001f;
    if (pwmDutyCycleUnique > 1.0f) pwmDutyCycleUnique = 1.0f;
    if (pwmDutyCycleUnique < 0.0f) pwmDutyCycleUnique = 0.0f;

    // update fans based on requested pwm
//    TIM3->CCR4 = (uint8_t) (pwmDutyCycleMain * 255.0f);
//    TIM3->CCR1 = (uint8_t) (pwmDutyCycleUnique * 255.0f);
  }
}

/*
void calculateTrueRpm(float deltaTime) {
  currTach = HAL_GPIO_ReadPin(Tach_from_Main_GPIO_Port, Tach_from_Main_Pin) == GPIO_PIN_SET;
  currTach2 = HAL_GPIO_ReadPin(Tach_from_Unique_GPIO_Port, Tach_from_Unique_Pin) == GPIO_PIN_SET;

  time += deltaTime;
  time2 += deltaTime;

  // Detects when tach falls from high to low, and stores pulse time for main and unique
  if (prevTach > currTach || time > tachTimeout) {
    if (time > tachTimeout) {
      time = 0.0f;
    }
    pulseTimes[index1] = time;
    time = 0.0f;
    index1++;
    index1 %= 10;
  }
  prevTach = currTach;

  if (prevTach2 > currTach2 || time2 > tachTimeout) {
    if (time2 > tachTimeout) {
      time2 = 0.0f;
    }
    pulseTimes2[index2] = time2;
    time2 = 0.0f;
    index2++;
    index2 %= 10;
  }
  prevTach2 = currTach2;

  // Adds up total pulse time and counts num of pulses to calculate true rpm for main and unique
  // TODO tends to give wonky numbers as fan RPM goes really low (i think it's related to the >= 5 thing, and i noticed it's better when the tachTimeout is larger)
  numPulses = 0;
  timeTotal = 0;
  for (float t: pulseTimes) {
    if (t > 0.0f) numPulses += 1;
    timeTotal += t;
  }
  trueRpmMain = (numPulses >= 5) ? (numPulses / timeTotal * 60.0f / 2.0f) : 0.0f;

  numPulses2 = 0;
  timeTotal2 = 0;
  for (float t: pulseTimes2) {
    if (t > 0.0f) numPulses2 += 1;
    timeTotal2 += t;
  }
  trueRpmUnique = (numPulses2 >= 5) ? (numPulses2 / timeTotal2 * 60.0f / 2.0f) : 0.0f;
}
*/

void fansInit() {
  can_addInbox(VCU_HVC_COOLING, &coolingInbox);

//  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
//  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

void fansPeriodic(float deltaTime) {
  if (coolingInbox.isRecent) {
    coolingInbox.isRecent = false;
    //reqPwm = (float) can_readInt(uint8_t, &coolingInbox, 0);
  }
  //if(pwmDutyCycleMain > 0.9f) reqPwm = 10.0f;
  //if(pwmDutyCycleMain < 0.1f) reqPwm = 90.0f;
//  reqPwm = 0.0f;
//  setFanPwm(reqPwm, deltaTime);
  //calculateTrueRpm(deltaTime);
}
