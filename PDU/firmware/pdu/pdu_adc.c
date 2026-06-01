#include "pdu_adc.h"

#include "adc.h"
#include "main.h"

#define ADC_MAX_VAL ((1u << 12) - 1u)
#define ADC_SCALE_V 3.2837f
#define ADC5_DMA_CHANNEL_COUNT 2u

// ADC5 DMA buffer: [0] = ADC5_IN1 (PA8 / SDWN1 LV), [1] = ADC5_IN2 (PA9 / BSE3)
static volatile uint16_t adc5_dma_buf[ADC5_DMA_CHANNEL_COUNT];

static void pdu_adc5_start_dma(void) {
    HAL_StatusTypeDef status =
        HAL_ADC_Start_DMA(&hadc5, (uint32_t *)adc5_dma_buf,
                          ADC5_DMA_CHANNEL_COUNT);

    if (status != HAL_OK && status != HAL_BUSY) {
        Error_Handler();
    }
}

void pdu_adc_init(void) {
    pdu_adc5_start_dma();
}

static float adc_counts_to_voltage(uint16_t adc_value) {
    return ((float)adc_value * ADC_SCALE_V) / ADC_MAX_VAL;
}

float pdu_adc5_sdwn1_voltage(void) {
    pdu_adc5_start_dma();
    return adc_counts_to_voltage(adc5_dma_buf[0]);
}

float pdu_adc5_bse3_voltage(void) {
    pdu_adc5_start_dma();
    return adc_counts_to_voltage(adc5_dma_buf[1]);
}
