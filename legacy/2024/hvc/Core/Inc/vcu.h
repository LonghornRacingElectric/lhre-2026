
#ifndef HVC_FIRMWARE_2024_VCU_H
#define HVC_FIRMWARE_2024_VCU_H

void vcuInit();

/**
 * gather IMU and pack data, send packets periodically
 */
void vcuPeriodic(bool amsIndicator, bool imdIndicator, int state, float deltaTime);


#endif //HVC_FIRMWARE_2024_VCU_H
