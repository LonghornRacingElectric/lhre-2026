#ifndef PDU_ADC_H
#define PDU_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

void pdu_adc_init(void);
float pdu_adc5_sdwn1_voltage(void);
float pdu_adc5_bse3_voltage(void);

#ifdef __cplusplus
}
#endif

#endif  // PDU_ADC_H
