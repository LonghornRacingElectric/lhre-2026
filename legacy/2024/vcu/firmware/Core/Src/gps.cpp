#include "gps.h"
#include "LonghornLib/faults.h"
#include "LonghornLib/clock.h"


Adafruit_GPS gps(huart1);
GpsData referenceGPSData;

Adafruit_GPS::Adafruit_GPS(UART_HandleTypeDef &hlpuart) : uart_handler(hlpuart) {
    this->uart_handler = hlpuart;

    paused = false;
    received = false;
    is_ready = false;
    hour = minute = seconds = year = month = day = fixquality = fixquality_3d = satellites = 0;
    latitude = longitude = geoidheight = altitude = HDOP = VDOP = PDOP = speed = angle = 0.0;
    latitude_dir = longitude_dir = 0;
    antenna = UNKNOWN;

    has_fix = false;
    milliseconds = 0;

    latitudeDegrees = longitudeDegrees = 0.0;
    latitude_fixed = longitude_fixed = 0;

    count = 0;
    countPerSecond = 0;
    lastTimeRecorded = 0;

    last_line.reserve(MAX_GPS_LINE_SIZE);
}

int Adafruit_GPS::send_command(const char *cmd) {
    std::string cmd_str = cmd;
    cmd_str.append("\r\n");
    if((cmd_str.substr(0, 5) != "$PMTK") && (cmd_str.substr(0, 4) != "$PGC")) {
        return 1;
    }
    return HAL_UART_Transmit(&uart_handler, (uint8_t *) cmd_str.c_str(), cmd_str.size(), 100);
}

char Adafruit_GPS::read_command(){
    if(!gps_completeLine || paused){
        return 0;
    }
    last_line = gps_currLine;
    gps_completeLine = false;
    received = true;
    return last_line[0];
}

bool Adafruit_GPS::newNMEAreceived() const {
    return received;
}

bool Adafruit_GPS::pause(bool p) {
    paused = p;
    if(paused){
        HAL_UART_DMAPause(&uart_handler);
    } else {
        waitForNewMessage();
    }
    return paused;
}

int Adafruit_GPS::lastNMEA(vector<string>& nmea, int max) {
    received = false;
    if(max < 2) return -1;
    size_t start = 0, end = 0;
    for(int i = 1; i <= max; i++){
        end = last_line.find("\r\n", start);
        if(end == string::npos){
            return i;
        }
        nmea.push_back(last_line.substr(start, end - start));
        start = end + 2;

    }
    return 0;
}

int Adafruit_GPS::waitForNewMessage() {
  auto error = HAL_UARTEx_ReceiveToIdle_DMA(&uart_handler, (uint8_t *) gps_tempLine, MAX_GPS_LINE_SIZE);
  if (error != HAL_OK) {
    FAULT_SET(&faultVector, FAULT_VCU_GPS_NO_DMA_START);
  }
  return error;
}

bool Adafruit_GPS::checkTimeout(){
  if(clock_getTime() - gps.lastTimeRecorded > 1.0f){
    gps.countPerSecond = gps.count;
    gps.count = 0;
    gps.lastTimeRecorded = clock_getTime();
    if(gps.countPerSecond == 0) {
      FAULT_SET(&faultVector, FAULT_VCU_GPS_TIMEOUT);
      return true;
    }
    FAULT_CLEAR(&faultVector, FAULT_VCU_GPS_TIMEOUT);
  }
  return false;
}

void gps_init() {
    gps.waitForNewMessage();
    // Baud rate is hard-coded to 115200 bps
    // Note: we may have to connect an arduino to the GPS module to change the baud rate
    bool fault = false;
    auto status = gps.send_command(PMTK_SET_BAUD_115200);
    if(status != HAL_OK) {
      fault = true;
    }
    //sends both GGA and RMC data
    status = gps.send_command(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    if(status != HAL_OK) {
      fault = true;
    }
    //sets the update rate to 1 Hz
    status = gps.send_command(PMTK_SET_NMEA_UPDATE_10HZ);
    if(status != HAL_OK) {
      fault = true;
    }
    //requests the antenna status
    status = gps.send_command(PGCMD_ANTENNA);
    if(status != HAL_OK) {
      fault = true;
    }
    gps.is_ready = !fault;
}

void gps_periodic(GpsData* gpsData) {
    // Checks for timeout, if it has been a second without receiving data, then restart DMA
    if(gps.checkTimeout()){
        HAL_UART_DMAStop(&huart1);
        gps.waitForNewMessage();
        return;
    }
    gps.read_command();
    if(gps.newNMEAreceived()){
        vector<string> new_lines;
        int error = gps.lastNMEA(new_lines, 2);
        if(error != 0) {
          FAULT_SET(&faultVector, FAULT_VCU_GPS_BAD_RX);
          return;
        }
        for(const auto& new_line : new_lines) {
          if (!gps.parse(new_line)) {
            return;
          }
        }
        gps.count++;
        if(gps.has_fix){
            gpsData->latitude = gps.latitudeDegrees;
            gpsData->longitude = gps.longitudeDegrees;
            gpsData->speed = gps.speed;
            gpsData->heading = gps.angle;
            gpsData->hour = gps.hour;
            gpsData->minute = gps.minute;
            gpsData->seconds = gps.seconds;
            gpsData->year = gps.year;
            gpsData->month = gps.month;
            gpsData->day = gps.day;
            gpsData->millis = gps.milliseconds;

            /* Set the reference GPS data if it has not been set yet */
            if(referenceGPSData.year == 0 && referenceGPSData.month == 0 && referenceGPSData.day == 0){
                referenceGPSData = *gpsData;
            }
        }
    }
}
