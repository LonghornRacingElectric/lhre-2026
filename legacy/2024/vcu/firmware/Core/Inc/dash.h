#ifndef VCU_FIRMWARE_2024_DASH_H
#define VCU_FIRMWARE_2024_DASH_H

#include "VcuModel.h"
#include "pdu.h"
#include "hvc.h"
#include "inverter.h"
#include "gps.h"

/**
 * Initialize CAN outboxes for dash.
 */
void dash_init();

/**
 * Update CAN packets that go to the dash.
 */
void dash_periodic(PduStatus* pduStatus, HvcStatus* hvcStatus, InverterStatus* inverterStatus,
                   GpsData* gpsData, VcuOutput* vcuCoreOutput);

#endif //VCU_FIRMWARE_2024_DASH_H
