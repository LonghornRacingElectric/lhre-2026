#include "vcu.h"
#include "VcuModel.h"

void vcu_init(VcuParameters& params) {
  vcu_updateParameters(params);
}

void vcu_execute(AnalogVoltages &analogVoltages, DriveSwitchState &driveSwitchState, HvcStatus &hvcStatus,
                 PduStatus &pduStatus, InverterStatus &inverterStatus, WheelMagnetValues &wheelMagnetValues,
                 ImuData &imuData, GpsData &gpsData, VcuOutput &vcuOutput, float deltaTime) {

  bool inverterReady = (!hvcStatus.ams) && (!hvcStatus.imd) && (hvcStatus.contactorStatus == 3);


  if(deltaTime <= 0) {
    deltaTime = 0.003f;
  }
  vcuInput = {
    analogVoltages.apps1,
    analogVoltages.apps2,

    analogVoltages.bse1,
    analogVoltages.bse2,

    analogVoltages.steer,

    inverterStatus.rpm,

    wheelMagnetValues.fl,
    wheelMagnetValues.fr,
    wheelMagnetValues.bl,
    wheelMagnetValues.br,

    inverterStatus.motorTemp,
    inverterStatus.inverterTemp,
    hvcStatus.packTempMean,

    hvcStatus.stateOfCharge,
    inverterReady,

    inverterStatus.voltage,
    inverterStatus.current,
    // hvcStatus.packVoltage,
    //          hvcStatus.packCurrent,

    pduStatus.lvVoltage,
    pduStatus.lvCurrent,

    driveSwitchState,

    gpsData.latitude,
    gpsData.longitude,
    gpsData.speed,
    gpsData.heading,

    imuData.accelVcu,
    imuData.accelHvc,
    imuData.accelPdu,
    imuData.gyroVcu,
    imuData.gyroHvc,
    imuData.gyroPdu,
  };
  vcuModel.evaluate(&vcuInput, &vcuOutput, deltaTime);
}

void vcu_updateParameters(VcuParameters& params) {
  vcuModel.setParameters(&params);
}
