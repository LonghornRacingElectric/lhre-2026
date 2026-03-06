#ifndef PDU_FIRMWARE_SWITCHES_H
#define PDU_FIRMWARE_SWITCHES_H

void switches_init();

void switches_setBrakeLight(float f);
void switches_setPump(float f);
void switches_setAccessory(float f);
void switches_setRadiatorFans(float f);
void switches_setBatteryFans(bool on);
void switches_setGLV(bool on);
void switches_setShutdown(bool on);

#endif //PDU_FIRMWARE_SWITCHES_H
