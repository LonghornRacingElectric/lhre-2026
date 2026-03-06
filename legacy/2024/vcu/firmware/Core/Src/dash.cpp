#include "dash.h"
#include "LonghornLib/angel_can.h"
#include "gps.h"
#include "all_imus.h"
#include "inverter.h"

CanOutbox dashOutbox1;
CanOutbox dashOutbox2;

void dash_init() {
  can_addOutbox(VCU_DASH_INFO1, 0.01f, &dashOutbox1);
  can_addOutbox(VCU_DASH_INFO2, 0.01f, &dashOutbox2);
}

void dash_periodic(PduStatus* pduStatus, HvcStatus* hvcStatus, InverterStatus* inverterStatus, GpsData* gpsData,
                   VcuOutput* vcuCoreOutput) {
  dashOutbox1.dlc = 8;
  dashOutbox1.data[0] = (int8_t) (gpsData->speed * 1.15078f);
  // dashOutbox1.data[0] = (int8_t) (vcuCoreOutput->dashSpeed);

  dashOutbox1.data[1] = (uint8_t) (inverterStatus->voltage * inverterStatus->current / 1000.0f);
  dashOutbox1.data[2] = (uint8_t) (vcuCoreOutput->telemetryApps * 100.0f);
  dashOutbox1.data[3] = (uint8_t) (vcuCoreOutput->telemetryBse * 100.0f);
  dashOutbox1.data[4] = (uint8_t) (vcuCoreOutput->hvBatterySoc * 100.0f);
  dashOutbox1.data[5] = (uint8_t) (vcuCoreOutput->lvBatterySoc * 100.0f);
  dashOutbox1.data[6] = (uint8_t) (hvcStatus->packTempMax);
  dashOutbox1.data[7] = (uint8_t) (inverterStatus->motorTemp);

  dashOutbox2.dlc = 8;
  dashOutbox2.data[0] = gpsData->hour; // lap time minutes
  dashOutbox2.data[1] = gpsData->minute; // lap time seconds (0-59)
  dashOutbox2.data[2] = gpsData->seconds; // lap time hundredths (0-99)
  dashOutbox2.data[3] = gpsData->millis / 100; // lap time millis (0-9)
  dashOutbox2.data[4] = false; // wheel speed error
  dashOutbox2.data[5] = false; // vcu error
  dashOutbox2.data[6] = false; // inverter error
  dashOutbox2.data[7] = false; // cooling error
}
