#ifndef VCU_FIRMWARE_2024_ADAFRUIT_DEFINES_H
#define VCU_FIRMWARE_2024_ADAFRUIT_DEFINES_H

/**
 Different commands to set the update rate from once a second (1 Hz) to 10 times
 a second (10Hz) Note that these only control the rate at which the position is
 echoed, to actually speed up the position fix you must also send one of the
 position fix rate commands below too. */
#define PMTK_SET_NMEA_UPDATE_100_MILLIHERTZ                                    \
  "$PMTK220,10000*2F" ///< Once every 10 seconds, 100 millihertz.
#define PMTK_SET_NMEA_UPDATE_200_MILLIHERTZ                                    \
  "$PMTK220,5000*1B" ///< Once every 5 seconds, 200 millihertz.
#define PMTK_SET_NMEA_UPDATE_1HZ "$PMTK220,1000*1F" ///<  1 Hz
#define PMTK_SET_NMEA_UPDATE_2HZ "$PMTK220,500*2B"  ///<  2 Hz
#define PMTK_SET_NMEA_UPDATE_5HZ "$PMTK220,200*2C"  ///<  5 Hz
#define PMTK_SET_NMEA_UPDATE_10HZ "$PMTK220,100*2F" ///< 10 Hz
// Position fix update rate commands.
#define PMTK_API_SET_FIX_CTL_100_MILLIHERTZ                                    \
  "$PMTK300,10000,0,0,0,0*2C" ///< Once every 10 seconds, 100 millihertz.
#define PMTK_API_SET_FIX_CTL_200_MILLIHERTZ                                    \
  "$PMTK300,5000,0,0,0,0*18" ///< Once every 5 seconds, 200 millihertz.
#define PMTK_API_SET_FIX_CTL_1HZ "$PMTK300,1000,0,0,0,0*1C" ///< 1 Hz
#define PMTK_API_SET_FIX_CTL_5HZ "$PMTK300,200,0,0,0,0*2F"  ///< 5 Hz
// Can't fix position faster than 5 times a second!

#define PMTK_SET_BAUD_115200 "$PMTK251,115200*1F" ///< 115200 bps
#define PMTK_SET_BAUD_57600 "$PMTK251,57600*2C"   ///<  57600 bps
#define PMTK_SET_BAUD_9600 "$PMTK251,9600*17"     ///<   9600 bps

#define PMTK_SET_NMEA_OUTPUT_GLLONLY                                           \
  "$PMTK314,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on only the
///< GPGLL sentence
#define PMTK_SET_NMEA_OUTPUT_RMCONLY                                           \
  "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on only the
///< GPRMC sentenc
#define PMTK_SET_NMEA_OUTPUT_VTGONLY                                           \
  "$PMTK314,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on only the
///< GPVTG
#define PMTK_SET_NMEA_OUTPUT_GGAONLY                                           \
  "$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on just the
///< GPGGA
#define PMTK_SET_NMEA_OUTPUT_GSAONLY                                           \
  "$PMTK314,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on just the
///< GPGSA
#define PMTK_SET_NMEA_OUTPUT_GSVONLY                                           \
  "$PMTK314,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on just the
///< GPGSV
#define PMTK_SET_NMEA_OUTPUT_RMCGGA                                            \
  "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28" ///< turn on GPRMC and
///< GPGGA
#define PMTK_SET_NMEA_OUTPUT_RMCGGAGSA                                         \
  "$PMTK314,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29" ///< turn on GPRMC, GPGGA
///< and GPGSA
#define PMTK_SET_NMEA_OUTPUT_ALLDATA                                           \
  "$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28" ///< turn on ALL THE DATA
#define PMTK_SET_NMEA_OUTPUT_OFF                                               \
  "$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28" ///< turn off output

// to generate your own sentences, check out the MTK command datasheet and use a
// checksum calculator such as the awesome
// http://www.hhhh.org/wiml/proj/nmeaxor.html

#define PMTK_LOCUS_STARTLOG "$PMTK185,0*22" ///< Start logging data
#define PMTK_LOCUS_STOPLOG "$PMTK185,1*23"  ///< Stop logging data
#define PMTK_LOCUS_STARTSTOPACK                                                \
  "$PMTK001,185,3*3C" ///< Acknowledge the start or stop command
#define PMTK_LOCUS_QUERY_STATUS "$PMTK183*38"  ///< Query the logging status
#define PMTK_LOCUS_ERASE_FLASH "$PMTK184,1*22" ///< Erase the log flash data
#define LOCUS_OVERLAP                                                          \
  0 ///< If flash is full, log will overwrite old data with new logs
#define LOCUS_FULLSTOP 1 ///< If flash is full, logging will stop

#define PMTK_ENABLE_SBAS                                                       \
  "$PMTK313,1*2E" ///< Enable search for SBAS satellite (only works with 1Hz
///< output rate)
#define PMTK_ENABLE_WAAS "$PMTK301,2*2E" ///< Use WAAS for DGPS correction data

#define PMTK_STANDBY                                                           \
  "$PMTK161,0*28" ///< standby command & boot successful message
#define PMTK_STANDBY_SUCCESS "$PMTK001,161,3*36" ///< Not needed currently
#define PMTK_AWAKE "$PMTK010,002*2D"             ///< Wake up

#define PMTK_Q_RELEASE "$PMTK605*31" ///< ask for the release and version

#define PGCMD_ANTENNA                                                          \
  "$PGCMD,33,1*6C" ///< request for updates on antenna status
#define PGCMD_NOANTENNA "$PGCMD,33,0*6D" ///< don't show antenna status messages

#define MAXWAITSENTENCE                                                        \
  10 ///< how long to wait when we're looking for a response
/**************************************************************************/

typedef enum {
    NMEA_HDOP = 0, ///< Horizontal Dilution of Position
    NMEA_LAT,      ///< Latitude in signed decimal degrees -90 to 90
    NMEA_LON,      ///< Longitude in signed decimal degrees -180 to 180
    NMEA_LATWP,    ///< Waypoint Latitude in signed decimal degrees -90 to 90
    NMEA_LONWP,    ///< Waypoint Longitude in signed decimal degrees -180 to 180
    NMEA_SOG,      ///< Speed over Ground in knots
    NMEA_COG,      ///< Course over ground, 0 to 360 degrees true
    NMEA_COG_SIN,  ///< sine of Course over ground
    NMEA_COG_COS,  ///< cosine of Course over ground
    NMEA_COGWP,    ///< Course over ground to the waypoint, 0 to 360 degrees true
    NMEA_XTE,      ///< Cross track error for the current segment to the waypoint,
    ///< Nautical Miles -ve to the left
    NMEA_DISTWP,   ///< Distance to the waypoint in nautical miles
    NMEA_AWA, ///< apparent wind angle relative to the boat -180 to 180 degrees
    NMEA_AWA_SIN, ///< sine of apparent wind angle relative to the boat
    NMEA_AWA_COS, ///< cosine of apparent wind angle relative to the boat
    NMEA_AWS,     ///< apparent wind speed, will be coerced to knots
    NMEA_TWA,     ///< true wind angle relative to the boat -180 to 180 degrees
    NMEA_TWA_SIN, ///< sine of true wind angle relative to the boat
    NMEA_TWA_COS, ///< cosine of true wind angle relative to the boat
    NMEA_TWD, ///< true wind compass direction, magnetic 0 to 360 degrees magnetic
    NMEA_TWD_SIN, ///< sine of true wind compass direction, magnetic
    NMEA_TWD_COS, ///< cosine of true wind compass direction, magnetic
    NMEA_TWS,     ///< true wind speed in knots TWS
    NMEA_VMG,     ///< velocity made good relative to the wind -ve means downwind,
    ///< knots
    NMEA_VMGWP,   ///< velocity made good relative to the waypoint, knots
    NMEA_HEEL,    ///< boat heel angle, -180 to 180 degrees to starboard
    NMEA_PITCH,   ///< boat pitch angle, -180 to 180 degrees bow up
    NMEA_HDG,     ///< magnetic heading, 0 to 360 degrees magnetic
    NMEA_HDG_SIN, ///< sine of magnetic heading
    NMEA_HDG_COS, ///< cosine of magnetic heading
    NMEA_HDT,     ///< true heading, 0 to 360 degrees true
    NMEA_HDT_SIN, ///< sine of true heading
    NMEA_HDT_COS, ///< cosine of true heading
    NMEA_VTW,     ///< Boat speed through the water in knots
    NMEA_LOG,     ///< Distance logged through the water in nautical miles
    NMEA_LOGR,    ///< Distance logged through the water in nautical miles since
    ///< reset
    NMEA_DEPTH,   ///< depth of water below the surface in metres
    NMEA_RPM_M1,  ///< rpm of motor 1
    NMEA_TEMPERATURE_M1,    ///< temperature of motor 1 in C
    NMEA_PRESSURE_M1,       ///< pressure of motor 1 in kPa
    NMEA_VOLTAGE_M1,        ///< voltage of motor 1 in Volts
    NMEA_CURRENT_M1,        ///< current of motor 1 in Amps
    NMEA_RPM_M2,            ///< rpm of motor 2
    NMEA_TEMPERATURE_M2,    ///< temperature of motor 2 in C
    NMEA_PRESSURE_M2,       ///< pressure of motor 2 in kPa
    NMEA_VOLTAGE_M2,        ///< voltage of motor 2 in Volts
    NMEA_CURRENT_M2,        ///< current of motor 2 in Amps
    NMEA_TEMPERATURE_AIR,   ///< outside temperature in C
    NMEA_TEMPERATURE_WATER, ///< sea water temperature in C
    NMEA_HUMIDITY,          ///< outside relative humidity in %
    NMEA_BAROMETER, ///< barometric pressure in Pa absolute -- not altitude
    ///< corrected
    NMEA_USR_00,    ///< spaces for a user sketch to inject its own data
    NMEA_USR_01,    ///< spaces for a user sketch to inject its own data
    NMEA_USR_02,    ///< spaces for a user sketch to inject its own data
    NMEA_USR_03,
    NMEA_USR_04,
    NMEA_USR_05,
    NMEA_USR_06,
    NMEA_USR_07,
    NMEA_USR_08,
    NMEA_USR_09,
    NMEA_USR_10,
    NMEA_USR_11,
    NMEA_USR_12,
    NMEA_MAX_INDEX ///< the largest number in the enum type -- not for data,
    ///< but does define size of data value array required.
} nmea_index_t;  ///< Indices for data values expected to change often with time


#endif //VCU_FIRMWARE_2024_ADAFRUIT_DEFINES_H
