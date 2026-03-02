/**
 * @file hvc_thermistors.h
 * @brief High Voltage Controller - Thermistor Temperature Sensing Interface
 * 
 * Provides temperature conversion for 10kΩ NTC thermistors in voltage divider configuration
 */

#ifndef HVC_THERMISTORS_H
#define HVC_THERMISTORS_H

#include <stdint.h>

/**
 * @brief Convert NTC thermistor voltage to temperature in Celsius
 * 
 * Converts voltage reading from a 10kΩ NTC thermistor in a voltage divider
 * circuit (3.3V -> 10kΩ resistor -> GPIO pin -> thermistor -> GND) to 
 * temperature in Celsius.
 * 
 * Uses the Beta equation with typical parameters for 10kΩ NTC thermistors:
 * - R0 = 10kΩ at T0 = 25°C
 * - Beta coefficient = 3950
 * 
 * @param voltage_v Measured voltage at GPIO pin (in volts)
 * @return Temperature in Celsius, or -999.0f on error (invalid voltage)
 */
float ntc_voltage_to_temp(float voltage_v);

#endif // HVC_THERMISTORS_H
