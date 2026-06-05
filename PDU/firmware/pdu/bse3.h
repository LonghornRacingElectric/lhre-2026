#ifndef PDU_BSE3_H
#define PDU_BSE3_H

#ifdef __cplusplus
extern "C" {
#endif

void bse3_init(void);
float bse3_voltage(void);
float bse3_sensor_voltage(void);
float bse3_pressure_psi(void);

#ifdef __cplusplus
}
#endif

#endif  // PDU_BSE3_H
