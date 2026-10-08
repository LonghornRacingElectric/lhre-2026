//
// Created by Kaitlyn on 1/8/2025.
//

#include "soc_estimation.h"
#include <stdbool.h>
#include "vct_sense.h"
#include "cells.h"

static float charge = 0.0f;

// get soc with current integration when tractive system on
static float getSocWithCurrent(float dt, float current) {
    charge += current * dt;
    return (charge / BATT_CAPACITY) * 100.0f;
}

static int binSearch(float *arr, int l, int r, float val) {
    int mid = (r - l) / 2;
    float midVal = arr[mid];

    if (r <= l) {
        if (arr[mid] == val) {
            return midVal;
        }
        if (arr[mid] > val) {
            return binSearch(arr, l, mid - 1, val);
        }
        if (arr[mid] < val) {
            return binSearch(arr, mid + 1, r, val);
        }
    }
    return mid;

}

// linear interpolation between two data points
static float linterp(float x1, float x2, float y1, float y2, float x) {
    return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

// get soc with voltage lookup table when tractive system off
static float getSocWithVoltage(float voltage) {
    voltage = voltage * (SCALE / 1.0f);

    // if voltage exceeds max/min vals, clamp
    if (voltage > arrX[0]) {
        return arrY[0];
    } else if (voltage < arrX[TBL_LEN - 1]) {
        return arrY[TBL_LEN - 1];
    }

    int index = binSearch(arrX, 0, (TBL_LEN - 1), voltage);

    float x1 = arrX[index];
    float x2 = arrX[index + 1];
    float y1 = arrY[index];
    float y2 = arrY[index + 1];
    return linterp(x1, x2, y1, y2, voltage);
}

// public function
float getSoc(float deltaTime) {
    float current = getTractiveCurrent();
    float voltage = getPackVoltageFromCells();
    float minCurrentBound = 0.05; // can assume that car is stationary

    bool inBounds = (current < minCurrentBound && current > -minCurrentBound);

    if (charge == 0.0f || inBounds) {
        return getSocWithVoltage(voltage);
    } else {
        return getSocWithCurrent(deltaTime, current);
    }
}

