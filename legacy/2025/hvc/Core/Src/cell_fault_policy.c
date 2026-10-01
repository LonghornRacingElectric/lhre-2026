#include "cell_fault_policy.h"

bool cell_fault_is_monitored(uint32_t cellIndex)
{
    return cellIndex < HVC_IGNORED_CELL_FIRST_INDEX ||
           cellIndex > HVC_IGNORED_CELL_LAST_INDEX;
}

bool thermistor_reading_is_valid(float temperatureC)
{
    return temperatureC >= HVC_MIN_VALID_TEMPERATURE_C &&
           temperatureC <= HVC_MAX_VALID_TEMPERATURE_C;
}

bool thermistor_reading_is_overtemperature(float temperatureC,
                                            float overtemperatureLimitC)
{
    return thermistor_reading_is_valid(temperatureC) &&
           temperatureC > overtemperatureLimitC;
}
