/**
 * @file hvc_thermistors.c
 * @brief High Voltage Controller - Thermistor Temperature Sensing Implementation
 * 
 * Implements temperature conversion for 10kΩ NTC thermistors
 */

#include "hvc_thermistors.h"
#include <math.h>

// Thermistor circuit constants
#define VCC 3.3f              // Supply voltage
#define R_FIXED 10000.0f      // Fixed resistor value (10kΩ)
#define R0 10000.0f           // Thermistor resistance at T0 (10kΩ at 25°C)
#define T0 298.15f            // Reference temperature (25°C in Kelvin)
#define BETA 3950.0f          // Beta coefficient for typical 10k NTC thermistor

/**
 * @brief Convert NTC thermistor voltage to temperature in Celsius
 * 
 * Circuit: 3.3V -> 10kΩ resistor -> GPIO pin -> NTC thermistor -> GND
 * 
 * @param voltage_v Measured voltage at GPIO pin (in volts)
 * @return Temperature in Celsius, or -999.0f on error
 */
float ntc_voltage_to_temp(float voltage_v)
{
    // Validate input voltage
    if (voltage_v <= 0.0f || voltage_v >= VCC) {
        return -999.0f;  // Invalid voltage reading
    }
    
    // Calculate thermistor resistance from voltage divider
    // V_measured = VCC * R_thermistor / (R_fixed + R_thermistor)
    // Solving for R_thermistor:
    // R_thermistor = R_fixed * V_measured / (VCC - V_measured)
    float r_thermistor = R_FIXED * voltage_v / (VCC - voltage_v);
    
    // Apply Beta equation to convert resistance to temperature
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float steinhart = (1.0f / T0) + (1.0f / BETA) * logf(r_thermistor / R0);
    float temp_k = 1.0f / steinhart;  // Temperature in Kelvin
    
    // Convert to Celsius
    return temp_k - 273.15f;
}
