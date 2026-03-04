#include <cstring>
#include "cellular.h"
#include <queue>
#include "LonghornLib/faults.h"
#include "usb.h"
// #include "secrets.h"

//#define TEXTS_ONLY


volatile int x = 0;


// 0 = Did not Power On,
#define STATE_OFF 0
// 1 = Powered On but no connection
#define STATE_BOOTING 1
// 2 = Need to search for TMOBILE
#define STATE_SEARCHING 2
// 3 = Connect to TMOBILE but not server
#define STATE_CONNECTING 3
// 4 = Connected to server but did not send handshake
#define STATE_CONNECTED_NO_HANDSHAKE 4
// 5 = sent handshake, running as normal
#define STATE_OK 5
uint8_t cellular_systemState = STATE_OFF;
std::string cellular_debugMessage = "";

static float handshakeTimestamp;
static float powerOnStartTime;
static bool finished_tx = true;
static bool dmaDisable = true;
static std::queue<std::string> cellular_dataToSend;
static std::string expectedGlobalMQTTResponse = "";


// private helper methods
static int cellular_send(std::string *command) {
  std::string s = "send:";
//  println(s);
//  println(*command);
  auto bytes = reinterpret_cast<const uint8_t *>(command->c_str());
  uint32_t error = HAL_UART_Transmit(&huart7, bytes, command->size(), HAL_MAX_DELAY);
  return error;

}


static void cellular_sendNonBlocking(std::string &command) {
  if (!finished_tx) return;
  auto bytes = reinterpret_cast<const uint8_t *>(command.c_str());
  uint32_t error = HAL_UART_Transmit_DMA(&huart7, bytes, command.size());
  if (error == HAL_OK) {
    finished_tx = false;
  }


}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  finished_tx = true;
}

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static void cellular_split8(uint8_t *&arr, uint8_t num) {
  uint8_t *arr8 = reinterpret_cast<uint8_t *>(arr);
  *arr8 = num;
  arr += 1;
}

static void cellular_split16(uint8_t *&arr, uint16_t num) {
  uint16_t *arr16 = reinterpret_cast<uint16_t *>(arr);
  *arr16 = num;
  arr += 2;
}

static void cellular_split32(uint8_t *&arr, uint32_t num) {
  uint32_t *arr32 = reinterpret_cast<uint32_t *>(arr);
  *arr32 = num;
  arr += 4;
}


static int cellular_Base64encode(char *encoded, const uint8_t *string, int len) {
  int i;
  char *p;

  p = encoded;
  for (i = 0; i < len - 2; i += 3) {
    *p++ = basis_64[(string[i] >> 2) & 0x3F];
    *p++ = basis_64[((string[i] & 0x3) << 4) |
                    ((int) (string[i + 1] & 0xF0) >> 4)];
    *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                    ((int) (string[i + 2] & 0xC0) >> 6)];
    *p++ = basis_64[string[i + 2] & 0x3F];
  }
  if (i < len) {
    *p++ = basis_64[(string[i] >> 2) & 0x3F];
    if (i == (len - 1)) {
      *p++ = basis_64[((string[i] & 0x3) << 4)];
      *p++ = '=';
    } else {
      *p++ = basis_64[((string[i] & 0x3) << 4) |
                      ((int) (string[i + 1] & 0xF0) >> 4)];
      *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
    }
    *p++ = '=';
  }

  *p++ = '\0';
  return p - encoded;
}


static bool cellular_receive(std::string &expected, bool care, uint32_t timeout) {
  static char buffer[512];
  memset(buffer, 0, sizeof(buffer));
  HAL_UART_Receive(&huart7, (uint8_t *) buffer, expected.size(), timeout);
  x++;
  auto str = new std::string(buffer);
  // std::string s = "receive:";
  // println(s);
  // println(*str);
  if (*str != expected) {
    delete str;
    FAULT_SET(&faultVector, FAULT_VCU_CELL_BAD_RX);
    return false;
  }
  delete str;
  return true;
}

static void cellular_receiveAny(int size, std::string &response, int time) {
  static char buffer[512] = {};
  uint16_t rxAmount = 0;
  HAL_UARTEx_ReceiveToIdle(&huart7, (uint8_t *) buffer, 2, &rxAmount, time); // \r\n
  HAL_UARTEx_ReceiveToIdle(&huart7, (uint8_t *) (buffer + 2), size - 2, &rxAmount, time);
  response = std::string(buffer);
  std::string s = "receiveAny:";
//  println(s);
//  println(response);
}

static bool cellular_receiveNonBlocking(std::string &expectedResponse, std::string &response, bool care) {
  if (!cell_completeLine) {
    return false;
  }
  cell_completeLine = false;
  response = std::string(cell_currLine);
  if ((response != expectedResponse) && care) {
    FAULT_SET(&faultVector, FAULT_VCU_CELL_BAD_RX);
    return false;
  }
  return true;
}

static bool cellular_sendAndExpectOk(std::string *command) {
  bool success = (cellular_send(command) == 0);
  success &= cellular_receive(CELL_OK, true, 1000);
  return success;
}

static bool cellular_areParametersUpdated() {
  return false; // TODO implement Texas Tune
}

static void cellular_sendStartTime(GpsData *gpsData) {
  if (gpsData->year == 0) {
    return;
  }
  int data = gpsData->month;
  std::string date;
  if (data < 10) {
    date = "0" + std::to_string(data);
  } else {
    date = std::to_string(data);
  }

  std::string time = "20" + std::to_string(gpsData->year) + "-"
                     + date + "-";
  data = gpsData->day;
  if (data < 10) {
    date = "0" + std::to_string(data);
  } else {
    date = std::to_string(data);
  }
  time = time + date + "T";
  data = gpsData->hour;
  if (data < 10) {
    date = "0" + std::to_string(data);
  } else {
    date = std::to_string(data);
  }
  time = time + date + ":";
  data = gpsData->minute;
  if (data < 10) {
    date = "0" + std::to_string(data);
  } else {
    date = std::to_string(data);
  }
  time = time + date + ":";
  data = gpsData->seconds;
  if (data < 10) {
    date = "0" + std::to_string(data);
  } else {
    date = std::to_string(data);
  }
  data = gpsData->millis;
  time = time + date + "." + std::to_string(data);
  std::string command = "AT+UMQTTC=2,0,0,\"config/car\",\"" + time + "\"\r";
//  cellular_sendNonBlocking(command);
  cellular_send(&command);
  cellular_systemState = STATE_OK;
  handshakeTimestamp = clock_getTime();
}


static uint32_t cellular_calculateTimeDiff() {
  float timeSinceHandshake = clock_getTime() - handshakeTimestamp;
  uint32_t timeSinceHandshakeMillis = static_cast<uint32_t>(timeSinceHandshake * 1000.0f);
  return timeSinceHandshakeMillis;
}

static void cellular_sendTelemetryHigh(VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                                       PduStatus *pduStatus, InverterStatus *inverterStatus,
                                       AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                                       ImuData *imuData, GpsData *gpsData) {


  std::string dataToEncode = "";
  // byte size of data
  uint8_t arr[200];
  uint8_t *ptr = arr;

  // For HF

  // version number
  uint8_t oneByteU = 1;
  cellular_split8(ptr, oneByteU);

  // Real Time Clock

  uint32_t fourBytesU = cellular_calculateTimeDiff();
  cellular_split32(ptr, fourBytesU);


  // VCU TorqueRequest
  int16_t twoBytes = (int16_t) (vcuCoreOutput->inverterTorqueRequest / 0.1f);
  cellular_split16(ptr, twoBytes);

  // vcu flag
  uint16_t twoBytesU = vcuCoreOutput->flags; // I do not know where to get this data
  cellular_split16(ptr, twoBytesU);

  // vcu displacement x y z
  twoBytes = (int16_t) (vcuCoreOutput->vehicleDisplacement.x / 0.1f);
  cellular_split16(ptr, twoBytes);

  twoBytes = (int16_t) (vcuCoreOutput->vehicleDisplacement.y / 0.1f);
  cellular_split16(ptr, twoBytes);

  twoBytes = (int16_t) (vcuCoreOutput->vehicleDisplacement.z / 0.1f);
  cellular_split16(ptr, twoBytes);

  // VCU Velocity x y z
  twoBytes = (int16_t) (vcuCoreOutput->vehicleVelocity.x / 0.01f);
  cellular_split16(ptr, twoBytes);

  twoBytes = (int16_t) (vcuCoreOutput->vehicleVelocity.y / 0.01f);
  cellular_split16(ptr, twoBytes);

  twoBytes = (int16_t) (vcuCoreOutput->vehicleVelocity.z / 0.01f);
  cellular_split16(ptr, twoBytes);

  // VCU Acceleration
  twoBytes = (int16_t) (vcuCoreOutput->vehicleAcceleration.x / 0.001f);
  cellular_split16(ptr, twoBytes);

  twoBytes = (int16_t) (vcuCoreOutput->vehicleAcceleration.y / 0.001f);
  cellular_split16(ptr, twoBytes);

  twoBytes = (int16_t) (vcuCoreOutput->vehicleAcceleration.z / 0.001f);
  cellular_split16(ptr, twoBytes);


  // HV Pack voltage TEMPORARY CHANGED
  twoBytesU = (uint16_t) (hvcStatus->packVoltage / 0.01f);
  cellular_split16(ptr, twoBytesU);


  ///////////
  // HV Tractive Voltage
  twoBytesU = (uint16_t) (0.01); // Dont know :(
  cellular_split16(ptr, twoBytesU);

  // HV Current
  twoBytes = (int16_t) (hvcStatus->packCurrent / 0.01f);
  cellular_split16(ptr, twoBytes);

  // LV Voltage
  twoBytesU = (uint16_t) (pduStatus->lvVoltage / 0.01f);
  cellular_split16(ptr, twoBytesU);

  // LV Current
  twoBytes = (int16_t) (pduStatus->lvCurrent / 0.01f);
  cellular_split16(ptr, twoBytes);

  // contactor state
  oneByteU = (uint8_t) (hvcStatus->contactorStatus);
  cellular_split8(ptr, oneByteU);

  // average cell voltage
  twoBytesU = 0; // dont know
  cellular_split16(ptr, twoBytesU);

  // average Cell Temp
  oneByteU = (uint8_t) (hvcStatus->packTempMean);
  cellular_split8(ptr, oneByteU);

  // apps 1
  twoBytesU = (uint16_t) (analogVoltages->apps1 / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // apps 2
  twoBytesU = (uint16_t) (analogVoltages->apps2 / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // bse1
  twoBytesU = (uint16_t) (analogVoltages->bse1 / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // bse 2
  twoBytesU = (uint16_t) (analogVoltages->bse2 / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // sus 1
  twoBytesU = (uint16_t) (analogVoltages->sus1 / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // sus 2
  twoBytesU = (uint16_t) (analogVoltages->sus2 / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // steer
  twoBytesU = (uint16_t) (analogVoltages->steer / 0.001f);
  cellular_split16(ptr, twoBytesU);

  // gps Latitude
  int32_t fourBytes = (int32_t) ((gpsData->latitude / 0.0001f) * 60);
  cellular_split32(ptr, fourBytes);

  // gps longitude
  fourBytes = (int32_t) ((gpsData->longitude / 0.0001f) * 60);
  cellular_split32(ptr, fourBytes);

  // gps speed
  twoBytesU = (uint16_t) (gpsData->speed / 0.01f);
  cellular_split16(ptr, twoBytesU);

  // gps heading
  twoBytesU = (uint16_t) (gpsData->heading / 0.01f);
  cellular_split16(ptr, twoBytesU);



  // IMU Acceleration (x,y,z): accel1, accel2, accel3, Fl, Fr, Bl, Br
  twoBytes = (int16_t) (imuData->accelVcu.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelVcu.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelVcu.z / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelHvc.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelHvc.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelHvc.z / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelPdu.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelPdu.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelPdu.z / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelFl.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelFl.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelFl.z / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelFr.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelFr.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelFr.z / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelBl.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelBl.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelBl.z / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelBr.x / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelBr.y / 0.001f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->accelBr.z / 0.001f);
  cellular_split16(ptr, twoBytes);


  // IMU Gyro (x,y,z): gyro1, gyro2, gyro3
  twoBytes = (int16_t) (imuData->gyroVcu.x / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroVcu.y / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroVcu.z / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroHvc.x / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroHvc.y / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroHvc.z / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroPdu.x / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroPdu.y / 1);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (imuData->gyroPdu.z / 1);
  cellular_split16(ptr, twoBytes);


  // Wheel Speed: fl, fr, bl, br
  twoBytes = (int16_t) (vcuCoreOutput->telemetryWheelSpeedFl / 0.1f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (vcuCoreOutput->telemetryWheelSpeedFr / 0.1f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (vcuCoreOutput->telemetryWheelSpeedBl / 0.1f);
  cellular_split16(ptr, twoBytes);
  twoBytes = (int16_t) (vcuCoreOutput->telemetryWheelSpeedBr / 0.1f);
  cellular_split16(ptr, twoBytes);

  // inverter Voltage
  twoBytesU = (uint16_t) (inverterStatus->voltage / 0.01f);
  cellular_split16(ptr, twoBytesU);

  // inverter current
  int16_t inverterCurrent = (int16_t) (inverterStatus->current / 0.01f);
  cellular_split16(ptr, inverterCurrent);

  // inverter RPM
  twoBytesU = (uint16_t) (inverterStatus->rpm / 1);
  cellular_split16(ptr, twoBytesU);

  // inverter Torque
  twoBytes = (int16_t) (inverterStatus->torqueActual / 0.1f);
  cellular_split16(ptr, twoBytes);


  // now encoding data
  char encoded[220];
  cellular_Base64encode(encoded, arr, 164);
  std::string encodedString = string(encoded);
  std::string command = "AT+UMQTTC=2,0,0,\"/h\",\"" + encodedString + "\"\r";
  cellular_dataToSend.push(command);
//  std::string response = "\r\r\n+UMQTTC: 2,1\r\r\n\r\nOK\r\n";\
//  std::string actual;
//  cellular_sendNonBlocking(command);
  // cellular_send(&command);
  // cellular_receiveAny(500, response, 10000);

}


static void cellular_sendTelemetryLow(VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                                      PduStatus *pduStatus, InverterStatus *inverterStatus,
                                      AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                                      ImuData *imuData, GpsData *gpsData) {
  std::string dataToEncode = "";
  uint8_t arr[404];
  uint8_t *ptr = arr;

  uint8_t oneByteU = 1;
  cellular_split8(ptr, oneByteU);
  uint32_t uint32 = cellular_calculateTimeDiff();
  cellular_split32(ptr, uint32);

  uint32 = 1;
  cellular_split32(ptr, uint32);

  uint32 = 1;
  cellular_split32(ptr, uint32);

  uint16_t cellVoltages[140];
  uint8_t cellTemperature[90];

  for (int i = 0; i < 140; i++) {
    cellVoltages[i] = 4;
    cellular_split16(ptr, cellVoltages[i]);
  }

  for (int i = 0; i < 90; i++) {
    cellTemperature[i] = 5;
    cellular_split8(ptr, cellTemperature[i]);
  }

  uint16_t hvSOC = (uint16_t) (hvcStatus->stateOfCharge / 0.01f);
  cellular_split16(ptr, hvSOC);

  uint16_t lvSOC = (uint16_t) (pduStatus->lvSoC / 0.01f);
  cellular_split16(ptr, lvSOC);

  uint8_t ambientTemp = 0; // dont know
  cellular_split8(ptr, ambientTemp);

  uint8_t inverterTemp = inverterStatus->inverterTemp;
  cellular_split8(ptr, inverterTemp);

  uint8_t motorTemp = inverterStatus->motorTemp;
  cellular_split8(ptr, motorTemp);

  uint8_t waterMotorTemp = pduStatus->waterTempMotor;
  cellular_split8(ptr, waterMotorTemp);

  uint8_t waterInverterTemp = pduStatus->waterTempInverter;
  cellular_split8(ptr, waterInverterTemp);

  uint8_t waterRadiatorTemp = pduStatus->waterTempRadiator;
  cellular_split8(ptr, waterRadiatorTemp);

  // need clarification on this
  uint16_t radFanSet = 0;
  cellular_split16(ptr, radFanSet);

  uint16_t radFanSpeed = 0;
  cellular_split16(ptr, radFanSpeed);

  uint16_t batFanSet = 0;
  cellular_split16(ptr, batFanSet);

  uint16_t batFanSpeed = 0;
  cellular_split16(ptr, batFanSpeed);

  uint16_t waterFlowRate = 0;
  cellular_split16(ptr, waterFlowRate);


  char encoded[540];
  cellular_Base64encode(encoded, arr, 404);
  for (int i = 0; i < 540; i++) {
    dataToEncode = dataToEncode + encoded[i];
  }
  std::string command = "AT+UMQTTC=2,0,0,\"/l\",\"" + dataToEncode + "\"\r";
//  std::string response = "\r\r\n+UMQTTC: 2,1\r\r\n\r\nOK\r\n";
//  cellular_sendNonBlocking(command);
  cellular_dataToSend.push(command);
  // cellular_receive(response, false, 1000);

}

static void cellular_updateParameters(VcuParameters *vcuCoreParameters) {

}

static void cellular_disableEcho() {
  std::string command;
  std::string response;

  command = "ATE0\r";
  cellular_send(&command); // TODO this will fail first time because it echoes the first time lol
  cellular_receive(CELL_OK, false, 500);
}

static void cellular_setBaudRate() {
  std::string command;

  command = "AT+IPR=115200\r";
  cellular_sendAndExpectOk(&command);
}

static void cellular_testConnection() {
  std::string command;

  command = "AT\r";
//  cellular_sendAndExpectOk(&command); // TODO don't fail, try again
  cellular_send(&command);
  cellular_receive(CELL_OK, false, 500);
}


static void cellular_mqttInit() {
  std::string s = "mqtt init";
//    println(s);

  std::string command;
  std::string response;
  int error = 0;
  bool good = false;
  volatile int x = 0;

  command = "AT+UMQTT=0,\"Car\"\r";
  response = "\r\r\n+UMQTT: 0,1\r\r\n\r\nOK\r\n";
  error = cellular_send(&command);
  if (error != 0) {
    return;
  }
  cellular_receiveAny(128, response, 200);

  command = "AT+UMQTT=1,1883\r";
  response = "\r\r\n+UMQTT: 1,1\r\r\n\r\nOK\r\n";
  error = cellular_send(&command);
  if (error != 0) {
    return;
  }

  cellular_receiveAny(128, response, 200);
//  good = cellular_receive(response, true, 1000);
//  if(!good) {
//    return;
//  }

  // command = std::string("AT+UMQTT=2,\"") + AWS_SERVER + std::string("\",1883\r");
  // response = "\r\r\n+UMQTT: 2,1\r\r\n\r\nOK\r\n";
  // error = cellular_send(&command);
  if (error != 0) {
    return;
  }
  cellular_receiveAny(128, response, 200);
//  good = cellular_receive(response, true, 1000);
//  if(!good) {
//    return;
//  }


  command = "AT+UMQTT=10,3600\r";
  response = "\r\r\n+UMQTT: 10,1\r\r\n\r\nOK\r\n";
  error = cellular_send(&command);
  if (error != 0) {
    return;
  }
  cellular_receiveAny(128, response, 200);
//  good = cellular_receive(response, true, 1000);
//  if(!good) {
//    return;
//  }

  command = "AT+UMQTT=12,1\r";
  response = "\r\r\n+UMQTT: 12,1\r\r\n\r\nOK\r\n";
  error = cellular_send(&command);
  if (error != 0) {
    return;
  }
  cellular_receiveAny(128, response, 200);
//  good = cellular_receive(response, true, 1000);
//  if(!good) {
//    return;
//  }
  command = "AT+UMQTTC=1\r";
  response = "\r\r\n+UMQTTC: 1,1\r\r\n\r\nOK\r\n\r\r\n+UUMQTTC: 1,0\r\r\n";
  error = cellular_send(&command);
  if (error != 0) {
    return;
  }
  cellular_receiveAny(128, response, 2000);
//  good = cellular_receive(response, true, 5000);
//  if (!good) {
//    return;
//  }

  cellular_systemState = STATE_CONNECTED_NO_HANDSHAKE;
  dmaDisable = false;
  volatile int dmaError = HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
  dmaError++;


}

static void cellular_findTMobileHSNCode(std::string &code, std::string &response) {

  /*
   Response example:
   "\r\n+COPS: (1,\"Verizon\",\"Verizon\",\"311480\",7),
   (1,\"313 100\",\"313 100\",\"313100\",7),(1,\"T-Mobile\",\"T-Mobile\",\"310260\",7),
   (1,\"311 490\",\"311 490\",\"311490\",7),(2,\"AT&T\",\"AT&T\",\"310410\",7),
   (1,\"T-Mobile\",\"T-Mobile\",\"310260\",9),,(0,1,2,3,4),(0,1,2)\r\n\r\nOK\r\n"
   */



  for (int i = 0; i < response.size(); i++) {
    char currentChar = response[i];
    if (currentChar == 'T') {
      char nextChar = response[i + 1];
      char afterChar = response[i + 2];
      if ((nextChar == '-') & (afterChar == 'M')) {
        i = i + 2;
        int initialP = 0;
        int endP = 0;
        currentChar = response[i];
        while (!(std::isdigit(currentChar))) {
          i++;
          currentChar = response[i];
        }
        currentChar = response[i];
        initialP = i;
        while ((std::isdigit(currentChar))) {
          i++;
          currentChar = response[i];
        }
        endP = i;
        std::string hsn = "";
        for (int j = initialP; j < endP; j++) {
          hsn = hsn + response[j];
        }
        code = hsn;
        break;


      }

    }
  }
}

static void cellular_nonBlockingMQTTINIT(int commandSet) {
  std::string command;
  std::string response;
  int error = 0;
  bool good = false;

  if (commandSet == 0) {
    command = "AT+UMQTT=0,\"Car\"\r";
    expectedGlobalMQTTResponse = "\r\r\n+UMQTT: 0,1\r\r\n\r\nOK\r\n";
    cellular_sendNonBlocking(command);
  } else if (commandSet == 1) {
    command = "AT+UMQTT=1,1883\r";
    expectedGlobalMQTTResponse = "\r\r\n+UMQTT: 1,1\r\r\n\r\nOK\r\n";
    cellular_sendNonBlocking(command);
  } else if (commandSet == 2) {
    // command = std::string("AT+UMQTT=2,\"") + AWS_SERVER + std::string("\",1883\r");
    expectedGlobalMQTTResponse = "\r\r\n+UMQTT: 2,1\r\r\n\r\nOK\r\n";
    cellular_sendNonBlocking(command);
  } else if (commandSet == 3) {
    command = "AT+UMQTT=10,3600\r";
    expectedGlobalMQTTResponse = "\r\r\n+UMQTT: 10,1\r\r\n\r\nOK\r\n";
    cellular_sendNonBlocking(command);

  } else if (commandSet == 4) {
    command = "AT+UMQTT=12,1\r";
    expectedGlobalMQTTResponse = "\r\r\n+UMQTT: 12,1\r\r\n\r\nOK\r\n";
    cellular_sendNonBlocking(command);
  } else {
    command = "AT+UMQTTC=1\r";
    expectedGlobalMQTTResponse = "\r\r\n+UMQTTC: 1,1\r\r\n\r\nOK\r\n\r\r\n+UUMQTTC: 1,0\r\r\n";
    cellular_sendNonBlocking(command);
  }
}

static void cellular_registerTMobile() {
  // HAL_Delay(15000);
  std::string command = "AT+COPS?\r";
  // std::string command = "AT+COPS=1,2,\"310260\"\r";
  // std::string command = "AT+CGMI\r";
  std::string response;
  cellular_send(&command);
  cellular_receiveAny(500, response, 1000);
  println(response);
  cellular_debugMessage = response;
  for (int i = 0; i < response.size(); i++) {
    if (response[i] == 'T') {
      cellular_systemState = STATE_CONNECTING;
      return;
    }
  }
  std::string command2 = "AT+COPS=1,2,\"310260\"\r";
  println(command2);
  cellular_send(&command2);
  for(int i = 0; i < 10; i++)
  {
    HAL_Delay(1000);
    println(i / 10.0f);
  }
  // cellular_systemState = STATE_CONNECTING; // TODO bypassing with hardcoded HSN

  if (cellular_systemState == STATE_BOOTING) {
    // TODO currently give up on reconnecting using DMA
    // dmaDisable = false;
    // cellular_systemState = STATE_SEARCHING;
    // std::string command = "AT+COPS=?\r";
    // HAL_Delay(20);
    // HAL_UART_Abort(&huart7);
    // HAL_Delay(20);
    // cellular_sendNonBlocking(command);
    // volatile uint32_t error = HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
    // error++;
  }

}

static void cellular_subscribe() {
  std::string command = "AT+UMQTTC=4,0,\"#\"\r";

  cellular_send(&command);
  uint8_t buffer[256];
  memset(buffer, 0, 256);
  HAL_UART_Receive(&huart7, (uint8_t *) buffer, 256, 5000);

//    cellular_receiveAny(500, response, 10000);

}


static void cellular_poll() {
  std::string command = "AT+UMQTTC=6\r";
  std::string response;
  cellular_send(&command);
  uint8_t buffer[256];
  memset(buffer, 0, 256);
  HAL_UART_Receive(&huart7, (uint8_t *) buffer, 256, 500);
  std::string num_response = "";
  for (int i = 0; i < 256; i++) {

    uint8_t current = buffer[i];
    if (current == uint8_t('h')) {
      // TODO do something
    }
  }
}

extern VcuOutput vcuCoreOutput;
extern GpsData gpsData;

static void cellular_respondToText(std::string *senderPhoneNumber, std::string *message) {
  std::string latString;
  if (gpsData.latitude > 0) {
    latString = std::to_string(gpsData.latitude);
    latString += " N";
  } else {
    latString = std::to_string(-gpsData.latitude);
    latString += " S";
  }

  std::string longString;
  if (gpsData.longitude > 0) {
    longString = std::to_string(gpsData.longitude);
    longString += " E";
  } else {
    longString = std::to_string(-gpsData.longitude);
    longString += " W";
  }

  std::string response = "Hi, it's Angelique! I'm parked at " + latString + ", " + longString + ".";
  cellular_sendText(senderPhoneNumber, &response);
}

// public methods

void cellular_init()
{
  //  std::string s = "========= cell init ==========";
  //  println(s);

  HAL_GPIO_WritePin(CELL_PWR_GPIO_Port, CELL_PWR_Pin, GPIO_PIN_RESET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(CELL_PWR_GPIO_Port, CELL_PWR_Pin, GPIO_PIN_SET);
  // powerOnStartTime = clock_getTime();

  std::string nonsense = "ding dong";

  // TODO comment this shit
  for(int i = 0; i < 10; i++) {
    println(nonsense);
    HAL_Delay(1000);
  }
  println( nonsense);
  cellular_disableEcho();
  cellular_testConnection();
  cellular_disableEcho();
  cellular_disableEcho();
  cellular_disableEcho();
  cellular_testConnection();
  cellular_registerTMobile();
  // std::string response;
  // std::string command = "AT+COPS=1,2,\"310260\"\r";
  // cellular_send(&command);
  // cellular_receiveAny(500, response, 1000);
  // cellular_debugMessage = response;
  // println(response);

  // cellular_mqttInit();
}


int cellular_sendText(std::string *phoneNumber, std::string *message) {
  std::string command;
  std::string response;
  bool success;

  command = "AT+CMGF=1\r";
  success = cellular_sendAndExpectOk(&command);
  if (!success) {
    return 1;
  }

  command = "AT+CMGS=\"" + *phoneNumber + "\"\r";
  response = "\r\n> ";
  cellular_send(&command);
  cellular_receive(response, true, 5000);

  cellular_send(message);

  command = "\x1A"; // Ctrl+Z character to indicate end of message
  cellular_send(&command);

  cellular_receiveAny(64, response, 30000);
  uint8_t index = response.size() - 6;
  const char *okBegin = response.c_str() + index;
  if (strcmp(okBegin, "\r\nOK\r\n") != 0) {
    return 1;
  }
  return 0;
}

void cellular_respondToTexts() {
  std::string command;
  std::string response;
  std::string sender;
  std::string message;
  bool success;

#ifndef TEXTS_ONLY
  HAL_Delay(20);
  HAL_UART_Abort(&huart7);
  HAL_Delay(20);
  finished_tx = true;
#endif

  command = "AT+CMGF=1\r";
  success = cellular_sendAndExpectOk(&command);
  if (!success) {
#ifndef TEXTS_ONLY
    HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
#endif
    return;
  }

  command = "AT+CMGL\r";
  cellular_send(&command);
  cellular_receiveAny(2048, response, 1000);

  // delete read messages to save space
  command = "AT+CMGD=0,3\r";
  success = cellular_sendAndExpectOk(&command);
  if (!success) {
#ifndef TEXTS_ONLY
    HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
#endif
    return;
  }

  for (int i = 0; i < response.size();) {
    i += 2;
    if (i >= response.size() || (response.at(i) == 'O' && response.at(i + 1) == 'K')) {
      break;
    }
    if (response.at(i) != '+') {
      break;
    }
    int commas = 0;
    for (; i < response.size(); i++) {
      if (response.at(i) == ',') {
        commas++;
        if (commas == 2) {
          break;
        }
      }
    }
    i += 2;
    if (i >= response.size()) {
      break;
    }
    sender = "";
    for (; i < response.size(); i++) {
      if (response.at(i) == '\"') {
        break;
      }
      sender += response.at(i);
    }
    if (i >= response.size()) {
      break;
    }
    for (; i < response.size(); i++) {
      if (response.at(i) == '\n') {
        break;
      }
    }
    i += 1;
    if (i >= response.size()) {
      break;
    }
    message = "";
    for (; i < response.size(); i++) {
      if (response.at(i) == '\r') {
        break;
      }
      message += response.at(i);
    }
    if (i >= response.size()) {
      break;
    }

    cellular_respondToText(&sender, &message);
  }

#ifndef TEXTS_ONLY
  HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
#endif
}


void cellular_periodic(VcuParameters *vcuCoreParameters,
                       VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                       PduStatus *pduStatus, InverterStatus *inverterStatus,
                       AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                       ImuData *imuData, GpsData *gpsData) {

#ifdef TEXTS_ONLY
  if (vcuCoreOutput->prndlState) {
    // we're in drive

  } else {
    // we're in park
    static bool inited = false;
    static float lastTextTime = 0;

    if (!inited && (clock_getTime() - powerOnStartTime > 10.0f)) {
      cellular_disableEcho();
      cellular_testConnection();
      cellular_disableEcho();
      cellular_disableEcho();
      cellular_disableEcho();
      cellular_testConnection();
      inited = true;
      lastTextTime = clock_getTime();
    }

    if(inited) {
      // check and respond to text messages once per second
      float nowTextTime = clock_getTime();
      if (nowTextTime - lastTextTime > 1.0f) {
        lastTextTime = nowTextTime;
        cellular_respondToTexts();
      }
    }
  }
#else

  /*
   * if(not_connected) {
   * if(cell_complete_line) string urmom = cell_currline;
   * else return;
   * TMOBILE_idx = urmom.contains(TMOBILE)
   * if(TMOBILE_idx != npos) {
   * val_we_want urmom.substr(TMOBILE_idx, 10)
   * not_connected = true;
   * }
   * else return;
   * }
   */

  // did not power on!
  if (cellular_systemState == STATE_OFF) {
    FAULT_CLEAR(&faultVector, CELL_RESET_STATE);
    FAULT_SET(&faultVector, FAULT_VCU_CELL_NOCXN);
    float timeSinceStart = clock_getTime();
    if (timeSinceStart < 12.0f) {
      return;
    } else {
      cellular_systemState = STATE_BOOTING;
      // Powered ON! Now starting up cellular
      cellular_disableEcho();
      cellular_testConnection();
      cellular_disableEcho();
      cellular_disableEcho();
      cellular_disableEcho();
      cellular_testConnection();
//      cellular_systemState = STATE_SEARCHING;
//        volatile uint32_t error = HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
//        error++;
//        dmaDisable = false;
//        HAL_Delay(100);
////        std::string command = "AT+COPS?\r";
//        std::string command = "AT+COPS=?\r";
//        cellular_sendNonBlocking(command);

      cellular_registerTMobile();
      HAL_Delay(100);



//        cellular_systemState = STATE_BOOTING;
      // requesting current connection information
//        std::string command = "AT+COPS?\r"; //"AT+COPS?\r";
//        cellular_sendNonBlocking(command);


    }
  }
//     waiting to check current connection info
//  else if (cellular_systemState == STATE_BOOTING) {
//      if (cell_completeLine & finished_tx) {
//          std::string response = cell_currLine;
//          cell_completeLine = false;
////          println(response);
//          for (int i = 0; i < response.size(); i++) {
//              if (response[i] == 'T') {
//                  cellular_systemState = STATE_CONNECTING;
//                  return;
//              }
//          }
//          if (cellular_systemState == STATE_BOOTING) {
//              cellular_systemState = STATE_SEARCHING;
//              std::string command = "AT+COPS=?\r";
//              cellular_sendNonBlocking(command);
//          }
//      }
////      if (finished_tx)
////      {
////          static float lastSearchingTime = 0;
////          float currentSearchingTime = clock_getTime();
////          if (currentSearchingTime - lastSearchingTime > 5.0f) {
////              lastSearchingTime = currentSearchingTime;
////              std::string command = "AT+COPS?\r";
////              cellular_sendNonBlocking(command);
//////              std::string response = "";
//////              cellular_receiveAny(100, response, 3.0f);
//////              println(response);
////          }
////      }
//
//
//  }
    // powered on but no connection
  else if (cellular_systemState == STATE_SEARCHING) {
    // TODO dont trust reconnection code
    // FAULT_CLEAR(&faultVector, CELL_RESET_STATE);
    // FAULT_SET(&faultVector, FAULT_VCU_CELL_SEARCHING);
    // if (cell_completeLine) {
    //   // cellular_debugMessage = cell_currLine;
    //   std::string availableConns = cell_currLine;
    //   cell_completeLine = false;
    //   std::string TMobile_HSNCode = "0";
    //   cellular_findTMobileHSNCode(TMobile_HSNCode, availableConns);
    //   if (TMobile_HSNCode != "0") {
    //     std::string command = "AT+COPS=1,2,\"" + TMobile_HSNCode + "\"" + "\r";
    //     cellular_sendNonBlocking(command);
    //     cellular_systemState = STATE_CONNECTING;
    //   }
    // }
    // static float lastSearchingTime = 0;
    // float currentSearchingTime = clock_getTime();
    // if (currentSearchingTime - lastSearchingTime > 180.0f) {
    //   lastSearchingTime = currentSearchingTime;
    //   std::string command = "AT+COPS=?\r";
    //   cellular_sendNonBlocking(command);
    // }
  }
    // has connection but no server = 3
  else if (cellular_systemState == STATE_CONNECTING) {
    FAULT_CLEAR(&faultVector, CELL_RESET_STATE);
    FAULT_SET(&faultVector, FAULT_VCU_CELL_CONNECTING);
    if (!dmaDisable) {
      if (finished_tx && !vcuCoreOutput->prndlState) {
        HAL_Delay(20);
        HAL_UART_Abort(&huart7);
        HAL_Delay(20);
        cellular_mqttInit();
        volatile uint32_t error = HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine, MAX_CELL_LINE_SIZE);
        error++;
        dmaDisable = false;
        HAL_Delay(100);
        finished_tx = true;
      }
//            static int commandSet = 0;
//            static bool commandSent = false;
//            if (!commandSent) {
//                if (finished_tx) {
//                    cellular_nonBlockingMQTTINIT(commandSet);
//                    commandSent = true;
//                }
//            } else {
//                bool care = commandSet == 5;
//                std::string check = "";
//                if (cellular_receiveNonBlocking(expectedGlobalMQTTResponse, check, care)) {
//                    if (commandSet == 5) {
//                        cellular_systemState = STATE_CONNECTED_NO_HANDSHAKE;
//                    }
//                    commandSent = false;
//                    commandSet++;
//                }
//            }

    } else {
      static float lastMqttAttempt = 0;
      if(clock_getTime() - lastMqttAttempt > 1.0f && !vcuCoreOutput->prndlState) {
        cellular_mqttInit();
        lastMqttAttempt = clock_getTime();
      }
    }

  } else if (cellular_systemState == STATE_CONNECTED_NO_HANDSHAKE) {
    FAULT_CLEAR(&faultVector, CELL_RESET_STATE);
    FAULT_SET(&faultVector, FAULT_VCU_CELL_NO_HANDSHAKE);
    if (dmaDisable) {
      volatile uint32_t error = HAL_UARTEx_ReceiveToIdle_DMA(&huart7, (uint8_t *) cell_tempLine,MAX_CELL_LINE_SIZE);
      if(error != HAL_OK) {
        FAULT_SET(&faultVector, FAULT_VCU_CELL_NO_DMA_START);
        return;
      }
      FAULT_CLEAR(&faultVector, FAULT_VCU_CELL_NO_DMA_START);
      error++;
      dmaDisable = false;
    } else if (finished_tx && !vcuCoreOutput->prndlState) {
      cellular_sendStartTime(gpsData);
    }

  } else { // cellular_systemState == STATE_OK
    FAULT_CLEAR(&faultVector, CELL_RESET_STATE);
    // generate high frequency message
    static float lastHFTime = clock_getTime();
    float nowHFTime = clock_getTime();
    if (nowHFTime - lastHFTime > 0.2f) { // TODO 20+ Hz
      lastHFTime = nowHFTime;
      if (cellular_dataToSend.size() < 10) {
        FAULT_CLEAR(&faultVector, FAULT_VCU_CELL_QUEUE_FULL);
        cellular_sendTelemetryHigh(vcuCoreOutput, hvcStatus,
                                   pduStatus, inverterStatus,
                                   analogVoltages, wheelMagnetValues,
                                   imuData, gpsData);
      } else {
        FAULT_SET(&faultVector, FAULT_VCU_CELL_QUEUE_FULL);
      }
    }

    // generate low frequency message
    static float lastLFTime = clock_getTime();
    float nowLFTime = clock_getTime();
    if (nowLFTime - lastLFTime > 1.0f) {
      lastLFTime = nowLFTime;
      if (cellular_dataToSend.size() < 10) {
        FAULT_CLEAR(&faultVector, FAULT_VCU_CELL_QUEUE_FULL);
//        cellular_sendTelemetryLow(vcuCoreOutput, hvcStatus,
//                                  pduStatus, inverterStatus,
//                                  analogVoltages, wheelMagnetValues,
//                                  imuData, gpsData);
      } else {
        FAULT_SET(&faultVector, FAULT_VCU_CELL_QUEUE_FULL);
      }
    }

    // send messages from queue
    static float lastQueuePopTime = clock_getTime() + 1.0f;
    float nowQueuePopTime = clock_getTime();
    if (finished_tx && (nowQueuePopTime - lastQueuePopTime > 0.030f) && !cellular_dataToSend.empty()) {
      lastQueuePopTime = nowQueuePopTime;
      std::string command = cellular_dataToSend.front();
      cellular_sendNonBlocking(command);
      cellular_dataToSend.pop();
    }


    // park/drive specific actions
    if (vcuCoreOutput->prndlState) {
      // we're in drive

    } else {
      // we're in park

      // check and respond to text messages once per second
      static float lastTextTime = clock_getTime() + 5.0f;
      float nowTextTime = clock_getTime();
      if (nowTextTime - lastTextTime > 1.0f && finished_tx) {
        lastTextTime = nowTextTime;
//         cellular_respondToTexts();
      }

      // check for new parameters from Texas Tune
//      if (cellular_areParametersUpdated()) {
//        cellular_updateParameters(vcuCoreParameters);
//      }
    }
  }
#endif
}
