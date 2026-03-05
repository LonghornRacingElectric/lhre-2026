#ifndef VCU_FIRMWARE_2024_CELLULAR_H
#define VCU_FIRMWARE_2024_CELLULAR_H

#include <string>
#include "VcuParameters.h"
#include "main.h"
#include "VcuModel.h"
#include "hvc.h"
#include "pdu.h"
#include "inverter.h"
#include "adc.h"
#include "wheelspeeds.h"
#include "all_imus.h"
#include "gps.h"
#include "LonghornLib/clock.h"


extern UART_HandleTypeDef huart7;
extern uint8_t cellular_systemState;
extern std::string cellular_debugMessage;

// strings
static std::string CELL_OK = "\r\nOK\r\n";


// private helper methods
static int cellular_send(std::string* command);

static void cellular_subscribe();

static void cellular_poll();


static void cellular_createDummyHFSend(VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                                       PduStatus *pduStatus, InverterStatus *inverterStatus,
                                       AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                                       ImuData *imuData, GpsData *gpsData);

static bool cellular_receive(std::string& expected, bool care);

static void cellular_receiveAny(int size, std::string& response, int time);

static bool cellular_sendAndExpectOk(std::string* command);

static bool cellular_areParametersUpdated();

static int cellular_Base64encode(char *encoded, const uint8_t *string, int len);

static void cellular_split8(uint8_t*& arr, uint8_t num);

static void cellular_split16(uint8_t*& arr, uint16_t num);

static void cellular_split32(uint8_t*& arr, uint32_t num);

static void cellular_updateParameters(VcuParameters *vcuCoreParameters);

static void cellular_sendTelemetryHigh(VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                                       PduStatus *pduStatus, InverterStatus *inverterStatus,
                                       AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                                       ImuData *imuData, GpsData *gpsData);

static void cellular_sendTelemetryLow(VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                                       PduStatus *pduStatus, InverterStatus *inverterStatus,
                                       AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                                       ImuData *imuData, GpsData *gpsData);

static void cellular_disableEcho();

static void cellular_findTMobileHSNCode(std::string& code);

static void cellular_nonBlockingMQTTINIT(int commandSet);
/**
 * only needs to be done once, then it's stored in the module's NVM
 */
static void cellular_setBaudRate();

static void cellular_testConnection();

static void cellular_mqttInit();

static void cellular_registerTMobile();

static void cellular_respondToText(std::string* senderPhoneNumber, std::string* message);


// public methods
void cellular_init();

int cellular_sendText(std::string* phoneNumber, std::string* message);

void cellular_respondToTexts();

void cellular_periodic(VcuParameters *vcuCoreParameters,
                       VcuOutput *vcuCoreOutput, HvcStatus *hvcStatus,
                       PduStatus *pduStatus, InverterStatus *inverterStatus,
                       AnalogVoltages *analogVoltages, WheelMagnetValues *wheelMagnetValues,
                       ImuData *imuData, GpsData *gpsData);






#endif //VCU_FIRMWARE_2024_CELLULAR_H
