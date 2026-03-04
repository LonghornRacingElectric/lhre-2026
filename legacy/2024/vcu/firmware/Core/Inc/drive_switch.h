#ifndef VCU_FIRMWARE_2024_DRIVE_SWITCH_H
#define VCU_FIRMWARE_2024_DRIVE_SWITCH_H

typedef bool DriveSwitchState;

void driveSwitch_periodic(DriveSwitchState* driveSwitchState);

#endif //VCU_FIRMWARE_2024_DRIVE_SWITCH_H
