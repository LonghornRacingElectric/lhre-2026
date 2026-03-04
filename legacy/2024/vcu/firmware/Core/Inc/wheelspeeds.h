#ifndef VCU_FIRMWARE_2024_WHEELSPEEDS_H
#define VCU_FIRMWARE_2024_WHEELSPEEDS_H

typedef struct WheelMagnetValues { // Unit is milliTeslas
    float fl;
    float fr;
    float bl;
    float br;
} WheelMagnetValues;

/**
 * Subscribe to CAN messages.
 */
void wheelspeeds_init();

/**
 * Get latest wheel magnetic field values from CAN.
 */
void wheelspeeds_periodic(WheelMagnetValues* wheelMagnetValues);

#endif //VCU_FIRMWARE_2024_WHEELSPEEDS_H
