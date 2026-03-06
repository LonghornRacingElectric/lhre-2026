#include "bevo.h"

static CanOutbox gpsFrame1Outbox;
static CanOutbox gpsFrame2Outbox;
static CanOutbox gpsFrame3Outbox;

void bevo_init()
{
    can_addOutbox(GPS_FRAME_1, GPS_PERIOD, &gpsFrame1Outbox);
    can_addOutbox(GPS_FRAME_2, GPS_PERIOD, &gpsFrame2Outbox);
    can_addOutbox(GPS_FRAME_3, GPS_PERIOD, &gpsFrame3Outbox);

    gpsFrame1Outbox.dlc = 8;
    gpsFrame2Outbox.dlc = 8;
    gpsFrame3Outbox.dlc = 8;
}

void bevo_send(GpsData *g)
{
    // Frame 1: latitude, longitude
    can_writeFloat(int32_t, &gpsFrame1Outbox, 0, g->latitude,  1e-7f);
    can_writeFloat(int32_t, &gpsFrame1Outbox, 4, g->longitude, 1e-7f);

    // Frame 2: speed, heading, hour, minute
    can_writeFloat(int32_t, &gpsFrame2Outbox, 0, g->speed,   0.001f); // m/s → mm/s
    can_writeFloat(int16_t, &gpsFrame2Outbox, 4, g->heading, 0.01f);  // deg → 0.01 deg

    gpsFrame2Outbox.data[6] = (uint8_t)g->hour;
    gpsFrame2Outbox.data[7] = (uint8_t)g->minute;

    // Frame 3: date + millis
    can_writeFloat(int16_t, &gpsFrame3Outbox, 0, g->year,   1.0f);
    gpsFrame3Outbox.data[2] = (uint8_t)g->month;
    gpsFrame3Outbox.data[3] = (uint8_t)g->day;
    can_writeFloat(int16_t, &gpsFrame3Outbox, 4, g->millis, 1.0f);
}

