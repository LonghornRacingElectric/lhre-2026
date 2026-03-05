//
// Created by Angela Suresh on 1/14/24.
//

#include "isense.h"
#include "cells.h"
#include "soc_estimation.h"


using namespace std;


static float battery_rated_capacity = 40407.84f * 133.0f / 140.0f; // 13920mAh at 587.58 V => 13.92 Ah => 50112 Coulombs
static float soc = -1.0f;

//when the tractive system is on
//not a public function
static float getSocWithCurrent(float dt, float outputCurr) {

  float chargeRemaining = (soc / 100.0f) * battery_rated_capacity;
  chargeRemaining += outputCurr * dt;
  float socNew = (chargeRemaining / battery_rated_capacity) * 100.0f;
  return socNew;
}

//not a public function
static int BinSearch(float *arr, int l, int r, float value) {
  //implement binary search of testX
  int middle = (l + r) / 2;
  if (r >= l) {
    if (arr[middle] == value) {
      return middle;
    }
    if (arr[middle] > value) {
      return BinSearch(arr, middle + 1, r, value);
    }
    if (arr[middle] < value) {
      return BinSearch(arr, l, middle - 1, value);
    }
  }
  return middle;
}

//not a public function
static float linearInterpolation(float x1, float x2, float y1, float y2, float x) {
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

//not a public function
static float socCalculation(float voltage, float *arrX, float *arrY, int n) {
  voltage = voltage * 140.0f / 133.0f;

  //clamping
  if (voltage > arrX[0]) {
    return arrY[0];
  } else if (voltage < arrX[ARR_LEN - 1]) {
    return arrY[ARR_LEN - 1];
  }

  int index = BinSearch(arrX, 0, n - 1, voltage);
  //given the xy coordinates
  if (index == ARR_LEN - 1) {
    return arrY[index];
  }
  float x1 = arrX[index];
  float x2 = arrX[index + 1];
  float y1 = arrY[index];
  float y2 = arrY[index + 1];
  return linearInterpolation(x1, x2, y1, y2, voltage);
}

//when tractive system is off
//not a public function
static float getSocWithVoltage(float outputVoltage) {
  float voltage = outputVoltage;
  int n = ARR_LEN;
  float result = socCalculation(voltage, arrX, arrY, n);
//   (result == -1) ? cout << "Element is not present in array" : cout << "Element is present at index " << result;
  return result;
}

//this is the public function to call
float getSoC(float deltaTime) {
  bool isSocValid = (soc != -1.0f);
  float dt = deltaTime;

  float outputCurr = getPackCurrent();
  float outputVoltage = getPackVoltageFromCells();

  float currentBound = 0.05f; // can set bounds to something else

  bool withinBounds = (outputCurr < currentBound && outputCurr > -currentBound);
  if (!isSocValid || withinBounds) {
    soc = getSocWithVoltage(outputVoltage);
  } else {
    soc = getSocWithCurrent(dt, outputCurr);
  }
  return soc;
}




