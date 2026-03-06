#ifndef HVC_FIRMWARE_2024_CHARGING_H
#define HVC_FIRMWARE_2024_CHARGING_H

#include <stdint.h>

#define HVC_ELCON_CHARGE_COMMAND 0x1806E5F4
#define ELCON_HVC_CHARGE_STATUS 0x18FF50E5
#define HVC_DASH_STATUS 0x420

#define MAX_CHARGE_VOLTAGE 537.6f // 128 * 4.2
#define MAX_CHARGE_CURRENT 13.5f


/**
 * Read proximity pilot (PP) and debounce to see if charger is plugged in properly.
 */
static void readProximity(float deltaTime);

/**
 * This method BLOCKS FOR TWO SECONDS while it reads control pilot (CP) to see the current limit.
 */
static void readControlPilot();

static void sendElconCommand(float voltage, float current, bool control);

static void receiveElconStatus();

static void sendDashMessage(bool plug, bool shutdown, bool energized, bool ams, bool imd,
                            float packVoltage, float soc, float timeRemaining);


void chargingInit();

bool isChargerPluggedIn();

void chargingPeriodic(bool shutdown, bool energized, bool ams, bool imd, float packVoltage, float maxCellMargin, float deltaTime);

#endif //HVC_FIRMWARE_2024_CHARGING_H
