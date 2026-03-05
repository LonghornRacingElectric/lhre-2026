#include "gps.h"
#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>
using namespace std;

/**************************************************************************/
/*!
  @file NMEA_parse.cpp

  This is the Adafruit GPS library - the ultimate GPS library
  for the ultimate GPS module!

  Tested and works great with the Adafruit Ultimate GPS module
  using MTK33x9 chipset
  ------> http://www.adafruit.com/products/746

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  @author Limor Fried/Ladyada for Adafruit Industries.

  @copyright BSD license, check license.txt for more information
  All text above must be included in any redistribution
*/
/**************************************************************************/

/**************************************************************************/
/*!
    @brief Parse a standard NMEA string and update the relevant variables.
   Sentences start with a $, then a two character source identifier, then a
   three character sentence identifier that defines the format, then a comma and
   more comma separated fields defined by the sentence name. There are many
   sentences listed that are not yet supported, including proprietary sentences
   that start with P, like the $PMTK commands to the GPS modules. See the
   build() function and http://fort21.ru/download/NMEAdescription.pdf for
   sentence descriptions.

   Encapsulated data sentences are supported by NMEA-183, and start with !
   instead of $. https://gpsd.gitlab.io/gpsd/AIVDM.html provides details
   about encapsulated data sentences used in AIS.

    parse() permits, but does not require Carriage Return and Line Feed at the
   end of sentences. The end of the sentence is recognized by the * for the
   checksum. parse() will not recognize a sentence without a valid checksum.

   NMEA_EXTENSIONS must be defined in order to parse more than basic
   GPS module sentences.

    @param nmea Pointer to the NMEA string
    @return True if successfully parsed, false if fails check or parsing
*/
/**************************************************************************/
bool Adafruit_GPS::parse(const string& nmea) {
    if (!check(nmea))
        return false;
    // passed the check, so there's a valid source in thisSource and a valid sentence in thisSentence

    string token;
    string dir_token;
    stringstream parsed_str(nmea);
    getline(parsed_str, token, ',');

    // This may look inefficient, but an M0 will get down the list in about 1 us /
    // strcmp()! Put the GPS sentences from Adafruit_GPS at the top to make
    // pruning excess code easier. Otherwise, keep them alphabetical for ease of
    // reading.
    if (thisSentence == "GGA") { //************************************GGA
        // Adafruit from Actisense NGW-1  from SH CP150C
        // Gets time token from the string
        getline(parsed_str, token, ',');
        parseTime(token);

        //Gets latitude and longitude
        getline(parsed_str, token, ',');
        getline(parsed_str, dir_token, ',');
        // parse out both latitude and direction, then go to next field, or fail
        parseCoord(token, dir_token);

        getline(parsed_str, token, ',');
        getline(parsed_str, dir_token, ',');
        // parse out both longitude and direction, then go to next field, or fail
        parseCoord(token, dir_token);

        getline(parsed_str, token, ',');
        // parse out both latitude and direction, then go to next field, or fail
        if (!token.empty()) { // if it's a ',' (or a '*' at end of sentence) the value is not included
            fixquality = stoi(token);
            has_fix = fixquality > 0;
        }
        // p = strchr(p, ',') + 1; // then move on to the next
        getline(parsed_str, token, ',');
        // Most can just be parsed with atoi() or atof(), then move on to the next.
        if (!token.empty())
            satellites = stoi(token);
        getline(parsed_str, token, ',');
        if (!token.empty())
            HDOP = stof(token);
        getline(parsed_str, token, ',');
        if (!token.empty())
            altitude = stof(token);
        getline(parsed_str, token, ',');
        getline(parsed_str, token, ','); // Runs twice because the token is just the unit of meters 'M' lol
        if (!token.empty())
            geoidheight = stof(token); // skip the rest

    } else if (thisSentence == "RMC") { //*****************************RMC
        // in Adafruit from Actisense NGW-1 from SH CP150C
        getline(parsed_str, token, ',');
        parseTime(token);

        getline(parsed_str, token, ',');
        parseFix(token);

        getline(parsed_str, token, ',');
        getline(parsed_str, dir_token, ',');
        // parse out both latitude and direction, then go to next field, or fail
        parseCoord(token, dir_token);

        getline(parsed_str, token, ',');
        getline(parsed_str, dir_token, ',');
        // parse out both longitude and direction, then go to next field, or fail
        parseCoord(token, dir_token);

        getline(parsed_str, token, ',');
        if (!token.empty()) speed = stof(token);
        getline(parsed_str, token, ',');
        if (!token.empty()) angle = stof(token);
        getline(parsed_str, token, ',');
        if (!token.empty()) {
            uint32_t fulldate = stoi(token);
            day = fulldate / 10000;
            month = (fulldate % 10000) / 100;
            year = (fulldate % 100);
        } // skip the rest

    } else if (thisSentence == "GLL") { //*****************************GLL
        // in Adafruit from Actisense NGW-1 from SH CP150C
        // parse out both latitude and direction, then go to next field, or fail

        getline(parsed_str, token, ',');
        getline(parsed_str, dir_token, ',');
        parseCoord(token, dir_token);

        getline(parsed_str, token, ',');
        getline(parsed_str, dir_token, ',');
        // parse out both longitude and direction, then go to next field, or fail
        parseCoord(token, dir_token);

        getline(parsed_str, token, ',');
        parseTime(token);
        getline(parsed_str, token, ',');
        parseFix(token); // skip the rest

    } else if (thisSentence == "GSA") { //*****************************GSA
        // in Adafruit from Actisense NGW-1
        getline(parsed_str, token, ',');
        if (!token.empty())
            fixquality_3d = stoi(token);
        getline(parsed_str, token, ',');
        // skip 12 Satellite PDNs without interpreting them
        for (int i = 0; i < 12; i++)
            getline(parsed_str, token, ',');
        if (!token.empty())
            PDOP = stof(token);
        getline(parsed_str, token, ',');
        // parse out HDOP, we also parse this from the GGA sentence. Chipset should
        // report the same for both
        if (!token.empty()) HDOP = stof(token);
        getline(parsed_str, token, ',');
        if (!token.empty())
            VDOP = stof(token); // last before checksum

    } else if (thisSentence == "TOP") { //*****************************TOP
        // See:
        // https://learn.adafruit.com/adafruit-ultimate-gps-featherwing/antenna-options
        // There is an output sentence that will tell you the status of the
        // antenna. $PGTOP,11,x where x is the status number. If x is 3 that means
        // it is using the external antenna. If x is 2 it's using the internal
        getline(parsed_str, token, ',');
        parseAntenna(token);
    }

    else {
        return false; // didn't find the required sentence definition
    }

    // Record the successful parsing of where the last data came from and when
    lastSource = thisSource;
    lastSentence = thisSentence;
    // lastUpdate = millis();
    return true;
}

/**************************************************************************/
/*!
    @brief Check an NMEA string for basic format, valid source ID and valid
    and valid sentence ID. Update the values of thisCheck, thisSource and
    thisSentence.
    @param nmea Pointer to the NMEA string
    @return True if well formed, false if it has problems
*/
/**************************************************************************/
bool Adafruit_GPS::check(const string& nmea) {
    thisCheck = 0; // new check
    thisSentence.clear();
    thisSource.clear();
    if (nmea[0] != '$' && nmea[0] != '!')
        return false; // doesn't start with $ or !
    else
        thisCheck += NMEA_HAS_DOLLAR;
    auto lastAsteriskIdx = nmea.rfind('*');
    if(lastAsteriskIdx == string::npos)
        return false;
    else {
        auto checkSumStr = nmea.substr(lastAsteriskIdx + 1, 2);
        uint16_t checkSum = stoi(checkSumStr, nullptr, 16);
        for(size_t i = 1; i < lastAsteriskIdx; i++)
            checkSum ^= nmea[i];
        if (checkSum != 0)
            return false; // bad checksum :(
        else
            thisCheck += NMEA_HAS_CHECKSUM;
    }
    // extract source of variable length
    string parsed_str = nmea.substr(1);
    thisSource = tokenOnList(parsed_str, sources);
    if (!thisSource.empty()) {
        thisCheck += NMEA_HAS_SOURCE;
    } else
        return false;
    parsed_str.erase(0, thisSource.length()); // remove source from string
    // extract sentence id and check if parsed
    thisSentence = tokenOnList(parsed_str, sentences_parsed);
    if (!thisSentence.empty()) {
        thisCheck += NMEA_HAS_SENTENCE_P + NMEA_HAS_SENTENCE;
    } else { // check if known
        thisSentence = tokenOnList(parsed_str, sentences_known);
        if (!thisSentence.empty()) {
            thisCheck += NMEA_HAS_SENTENCE;
            return false; // known but not parsed
        } else {
            parseStr(thisSentence, parsed_str, NMEA_MAX_SENTENCE_ID);
            return false; // unknown
        }
    }
    return true; // passed all the tests
}

/**************************************************************************/
/*!
    @brief Check if a token at the start of a string is on a list.
    @param token Pointer to the string
    @param list A list of strings, with the final entry starting "ZZ"
    @return Pointer to the found token, or NULL if it fails
*/
/**************************************************************************/
string Adafruit_GPS::tokenOnList(string& token, const unordered_set<string>& list) {
    for(const auto & it : list) {
        if (token.find(it) == 0)
            return it;
    }

    return ""; // couldn't find a match
}

/**************************************************************************/
/*!
    @brief Check if an NMEA string is valid and is on a list, perhaps to
    decide if it should be passed to a particular NMEA device.
    @param nmea Pointer to the NMEA string
    @param list A list of strings, with the final entry "ZZ"
    @return True if on the list, false if it fails check or is not on the list
*/
/**************************************************************************/
bool Adafruit_GPS::onList(string& nmea, const unordered_set<string>& list) {
    if (!check(nmea)) // sets thisSentence if valid
        return false;   // not a valid sentence
    auto it = list.find(thisSentence);
    if(it != list.end())
        return true;
    else
    return false; // couldn't find a match
}

/**************************************************************************/
/*!
    @brief Parse a part of an NMEA string for lat or lon angle and direction.
    Works for either DDMM.mmmm,N (latitude) or DDDMM.mmmm,W (longitude) format.
    Insensitive to number of decimal places present. Only fills the variables
    if it succeeds and the variable pointer is not NULL. This allows calling
    to fill only the variables of interest. Does rudimentary validation on
    angle range.

    Supersedes private functions parseLat(), parseLon(), parseLatDir(),
    parseLonDir(), all previously called from parse().
    @param pStart Pointer to the location of the token in the NMEA string
    @param angle Pointer to the angle to fill with value in degrees/minutes as
      received from the GPS (DDDMM.MMMM), unsigned
    @param angle_fixed Pointer to the fix point version latitude in decimal
      degrees * 10000000, signed
    @param angleDegrees Pointer to the angle to fill with decimal degrees,
      signed. As actual double on SAMD, etc. resolution is better than the
      fixed point version.
    @param dir Pointer to character to fill the direction N/S/E/W
    @return true if successful, false if failed or no value
*/
/**************************************************************************/
bool Adafruit_GPS::parseCoord(string& mag, string& dir) {
    if (!mag.empty() && !dir.empty()) {
        auto e = mag.find('.');
        if(e == string::npos)
            return false;
        auto degreebuff = mag.substr(0, e); //Get DDDMM
        uint16_t dddmm = stoi(degreebuff);
        uint16_t degrees = dddmm / 100;
        uint16_t minutes = dddmm - degrees * 100;
        double decminutes = stof(mag.substr(e));

        if(dir.empty())
            return false;
        char nsew = dir[0];

        // set the various numerical formats to their values
        int32_t fixed = degrees * 10000000 + (uint32_t) ((minutes * 10000000) / 60) +
                     (decminutes * 10000000) / 60;
        double ang = (double) degrees * 100 + (double) minutes + decminutes;
        double deg = (double) fixed / (double)10000000.;
        if (nsew == 'S' || nsew == 'W') { // fixed and deg are signed, but DDDMM.mmmm is not
            fixed = -1 * fixed;
            deg = -1 * deg;
        }

        // reject directions that are not NSEW
        if (nsew != 'N' && nsew != 'S' && nsew != 'E' && nsew != 'W')
            return false;

        // reject angles that are out of range
        if (nsew == 'N' || nsew == 'S')
            if (abs(deg) > 90)
                return false;
        if (abs(deg) > 180)
            return false;

        if(nsew == 'N' || nsew == 'S'){
            latitude = ang;
            latitude_fixed = fixed;
            latitudeDegrees = deg;
            latitude_dir = nsew;
        } else if(nsew == 'E' || nsew == 'W'){
            longitude = ang;
            longitude_fixed = fixed;
            longitudeDegrees = deg;
            longitude_dir = nsew;
        } else
            return false;
    } else
        return false; // no number
    return true;
}

/**************************************************************************/
/*!
    @brief Parse a string token from pointer p to the next comma, asterisk
    or end of string.
    @param buff Pointer to the buffer to store the string in
    @param p Pointer into a string
    @param n Max permitted size of string including terminating 0
    @return Pointer to the string buffer
*/
/**************************************************************************/
string Adafruit_GPS::parseStr(string& buff, string& p, int n) {
    auto e = p.find(',');
    int len = 0;
    if(e != string::npos){
        len = min((int)e, n);
        buff = p.substr(0, len);
    } else {
        e = p.find('*');
        if(e != string::npos){
            len = min((int)e, n);
            buff = p.substr(0, len);
        } else {
            len = min((int)p.length(), n);
            buff = p.substr(0, len);
        }
    }
    return buff;
}

/**************************************************************************/
/*!
    @brief Parse a part of an NMEA string for time. Independent of number
    of decimal places after the '.'
    @param p Pointer to the location of the token in the NMEA string
    @return true if successful, false otherwise
*/
/**************************************************************************/
bool Adafruit_GPS::parseTime(string& p) {
    if(p.empty())
        return false;
    uint32_t time = stoi(p);
    hour = time / 10000;
    minute = (time % 10000) / 100;
    seconds = (time % 100);
    auto dec = p.find('.');
    if(dec != string::npos && (dec < p.length() - 1))
        milliseconds = stoi(p.substr(dec + 1, p.length() - dec - 1));
    else
        milliseconds = 0;
    return true;
}

/**************************************************************************/
/*!
    @brief Parse a part of an NMEA string for whether there is a fix
    @param p Pointer to the location of the token in the NMEA string
    @return True if we parsed it, false if it has invalid data
*/
/**************************************************************************/
bool Adafruit_GPS::parseFix(string& p) {
    if (!p.empty()) {
        if (p[0] == 'A') {
            has_fix = true;
        } else if (p[0] == 'V')
            has_fix = false;
        else
            return false;
        return true;
    }
    return false;
}

/**************************************************************************/
/*!
    @brief Parse a part of an NMEA string for antenna that is used
    @param p Pointer to the location of the token in the NMEA string
    @return 3=external 2=internal 1=there was an antenna short or problem
*/
/**************************************************************************/
bool Adafruit_GPS::parseAntenna(string& p) {
    if (!p.empty()) {
        if (p[0] == '3') {
            antenna = static_cast<AntennaStatus>(3);
        } else if (p[0] == '2') {
            antenna = static_cast<AntennaStatus>(2);
        } else if (p[0] == '1') {
            antenna = static_cast<AntennaStatus>(1);
        } else
            return false;
        return true;
    }
    return false;
}

/**************************************************************************/
/*!
    @brief Is the field empty, or should we try conversion? Won't work
    for a text field that starts with an asterisk or a comma, but that
    probably violates the NMEA-183 standard.
    @param pStart Pointer to the location of the token in the NMEA string
    @return true if empty field, false if something there
*/
/**************************************************************************/
bool Adafruit_GPS::isEmpty(char *pStart) {
    if (',' != *pStart && '*' != *pStart && pStart != NULL)
        return false;
    else
        return true;
}