
#ifndef HVC_FIRMWARE_2024_FANS_H
#define HVC_FIRMWARE_2024_FANS_H

/* Variables */
static float tachTimeout = 0.4f;

// PWM Control
static float reqPwm = 0.0f;
static float pwmTimer = 0.0f;
static float pwmDutyCycleMain = 0.0f;
static float pwmDutyCycleUnique = 0.0f;

// Main Fan
static float time;
static float trueRpmMain = 0.0f; // 8300 RPM max
static float pulseTimes[10];
static float timeTotal = 0.0f;
static float numPulses = 0.0f;
static int index1 = 0;
static bool prevTach = false;
static bool currTach = false;

//Unique Fan
static float time2;
static float trueRpmUnique = 0.0f; // 9000 RPM max
static float pulseTimes2[10];
static float timeTotal2 = 0.0f;
static float numPulses2 = 0.0f;
static int index2 = 0;
static bool prevTach2 = false;
static bool currTach2 = false;

/* Functions */
void fansInit();
void fansPeriodic(float deltaTime);
void setFanPwm(float reqPwm, float deltaTime);
void calculateTrueRpm(float deltaTime);

#endif //HVC_FIRMWARE_2024_FANS_H
