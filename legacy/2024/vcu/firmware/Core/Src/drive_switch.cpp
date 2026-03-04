#include "drive_switch.h"
#include "main.h"

void driveSwitch_periodic(DriveSwitchState* driveSwitchState) {
    *driveSwitchState = HAL_GPIO_ReadPin(DriveSwitch_GPIO_Port, DriveSwitch_Pin) == GPIO_PIN_RESET;
}
