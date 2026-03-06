//
// Created by Ashwin Kuppahally on 5/5/25.
//

#include "pilot.h"
#include <stdio.h>
#include <stdint.h>
#include "adc.h"

#define THRESHOLD 2048  // ~1.65V on 12-bit ADC (0-4095)
#define BUFFER_SIZE 1
#define LOW_CURRENT_DUTY 0.3
#define WALL_CHARGING_CURRENT 2.5
#define CHARGEPOINT_CURRENT 10.0

uint16_t adc_buffer[BUFFER_SIZE];  // Fill this using DMA
float current=WALL_CHARGING_CURRENT;

void pilotInit()
{
  	// Start ADC in DMA mode
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, BUFFER_SIZE);
}

void pilotPeriodic()
{

	float duty_cycle = adc_buffer[BUFFER_SIZE-1]/3.3f;

    //Select current limit
	if(duty_cycle <= LOW_CURRENT_DUTY){
    	current=WALL_CHARGING_CURRENT;
    }
    else{
      current=CHARGEPOINT_CURRENT;
    }
}


float getCurrentLimit()
{
  return current;
}