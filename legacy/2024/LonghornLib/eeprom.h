#ifndef LONGHORN_LIBRARY_2024_EEPROM_H
#define LONGHORN_LIBRARY_2024_EEPROM_H

/**
 * Initialize EEPROM.
 */
void eeprom_init();

/**
 * Get a floating-point number stored at a given address in the EEPROM.
 * @param address Address counting from 0 where +1 is 1 float.
 * @return Floating-point number.
 */
float eeprom_getFloat(int address);

/**
 * Store a floating-point number at the given address in the EEPROM.
 * @param address Address counting from 0 where +1 is 1 float.
 * @param value Floating-point number.
 */
void eeprom_saveFloat(int address, float value);

#endif //LONGHORN_LIBRARY_2024_EEPROM_H
