#ifndef VCU_FIRMWARE_2024_GPS_H
#define VCU_FIRMWARE_2024_GPS_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include "usart.h"
#include "adafruit_defines.h"
using namespace std;

#define NMEA_MAX_SENTENCE_ID                                                   \
  20 ///< maximum length of a sentence ID name, including terminating 0
#define NMEA_MAX_SOURCE_ID                                                     \
  3 ///< maximum length of a source ID name, including terminating 0

/// type for resulting code from running check()
typedef enum {
    NMEA_BAD = 0, ///< passed none of the checks
    NMEA_HAS_DOLLAR =
    1, ///< has a dollar sign or exclamation mark in the first position
    NMEA_HAS_CHECKSUM = 2,   ///< has a valid checksum at the end
    NMEA_HAS_NAME = 4,       ///< there is a token after the $ followed by a comma
    NMEA_HAS_SOURCE = 10,    ///< has a recognized source ID
    NMEA_HAS_SENTENCE = 20,  ///< has a recognized sentence ID
    NMEA_HAS_SENTENCE_P = 40 ///< has a recognized parseable sentence ID
} nmea_check_t;

typedef enum {
    UNKNOWN = 0,
    PROBLEM = 1,
    INTERNAL = 2,
    EXTERNAL = 3
} AntennaStatus;

typedef struct GpsData {
    double latitude;
    double longitude;
    float speed;
    float heading;
    uint8_t hour, minute, seconds, year, month, day;
    uint16_t millis;
} GpsData;
extern GpsData referenceGPSData;

class Adafruit_GPS {
private:
    UART_HandleTypeDef& uart_handler;

    bool paused;

    bool received;

    const unordered_set<string> sources = {"II", "WI", "GP", "PG",
                              "GN", "P",  "ZZZ"}; ///< valid source ids
    const unordered_set<string> sentences_parsed = {"GGA", "GLL", "GSA", "RMC",
                                       "TOP", "ZZZ"}; ///< parseable sentence ids
    const unordered_set<string> sentences_known = {"DBT", "HDM", "HDT",
                                      "ZZZ"}; ///< known, but not parseable

    uint32_t countPerSecond;
    double lastTimeRecorded;

    int thisCheck = 0; //<the results of the check on the current sentence
    string thisSource;
    string thisSentence;
    string lastSource;
    string lastSentence;

    bool check(const string& nmea);
    static string tokenOnList(string& token, const unordered_set<string>& list);
    bool onList(string& nmea, const unordered_set<string>&list);
    bool parseCoord(string& p1, string& p2);
    static string parseStr(string& buff, string& p, int n);
    bool parseTime(string& p);
    bool parseFix(string& p);
    bool parseAntenna(string& p);
    bool isEmpty(char *pStart);

public:

    uint8_t hour, minute, seconds, year, month, day;
    double latitude, longitude, geoidheight, altitude;
    float HDOP, VDOP, PDOP;
    float speed, angle;
    uint8_t satellites;
    char latitude_dir, longitude_dir;
    uint8_t fixquality, fixquality_3d;
    AntennaStatus antenna;

    double latitudeDegrees;
    double longitudeDegrees;

    int32_t latitude_fixed;
    int32_t longitude_fixed;

    bool has_fix;
    bool is_ready;
    uint16_t milliseconds;

    string last_line;

    uint32_t count;

    explicit Adafruit_GPS(UART_HandleTypeDef &uart_handler);
    virtual ~Adafruit_GPS() = default;
    int send_command(const char *cmd);
    int waitForNewMessage();
    /**
     * Checks whether we received the next NMEA sentence from the GPS module
     * @return true or false
     */
    bool newNMEAreceived() const;
    /**
     * Pauses the GPS module
     * @param p true to pause, false to resume
     * @return true if paused, false if resumed
     */
    bool pause(bool p);
    /**
     * Returns a copy of the most recent NMEA sentence, used for parsing
     * @return
     */
    int lastNMEA(vector<string> &nmea, int max = 5);
    /**
     * Parses the NMEA sentence and puts the values into the field variables
     * @param nmea the NMEA sentence
     * @return true if the sentence was parsed successfully, false if not
     */
    bool parse(const string& nmea);
    /**
     * Checks if there hasnt been any new NMEA sentences in the last second
     * @return true or false for timeout
     */
    static bool checkTimeout();
    /**
     * Reads the next NMEA sentence from the GPS module.
     * @return
     */
    char read_command();
};

void gps_init();

void gps_periodic(GpsData* gpsData);

#endif //VCU_FIRMWARE_2024_GPS_H
