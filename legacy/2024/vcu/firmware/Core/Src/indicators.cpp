#include "indicators.h"
#include "main.h"
#include "tim.h"

//static void indicators_playNote(float frequency, float beats) {
//  float total = beats * 1000.0f / (TEMPO / 60.0f);
//  if(frequency == NOTE_REST) {
//    TIM2->CCR1 = 0;
//  } else {
//    TIM2->ARR = (uint32_t) (1000000.0f / frequency);
//    TIM2->CCR1 = TIM2->ARR / 2;
//  }
//  HAL_Delay((uint32_t)(total * 0.9f));
//  TIM2->CCR1 = 0;
//  HAL_Delay((uint32_t)(total * 0.1f));
//}
//
//static void indicators_deepInTheHeartOfTexas() {
//  indicators_playNote(NOTE_C4, 1);
//  indicators_playNote(NOTE_F4, 2);
//  indicators_playNote(NOTE_F4, 2);
//  indicators_playNote(NOTE_A4, 3);
//
//  indicators_playNote(NOTE_C4, 1);
//  indicators_playNote(NOTE_F4, 2);
//  indicators_playNote(NOTE_F4, 2);
//  indicators_playNote(NOTE_A4, 4);
//
//  indicators_playNote(NOTE_REST, 1);
//  indicators_playNote(NOTE_C5, 1);
//  indicators_playNote(NOTE_C5, 1);
//  indicators_playNote(NOTE_C5, 1);
//  indicators_playNote(NOTE_C5, 3);
//  indicators_playNote(NOTE_D5, 1);
//  indicators_playNote(NOTE_A4, 1);
//  indicators_playNote(NOTE_G4, 6);
//
//  indicators_playNote(NOTE_C4, 1);
//  indicators_playNote(NOTE_E4, 2);
//  indicators_playNote(NOTE_E4, 2);
//  indicators_playNote(NOTE_G4, 3);
//
//  indicators_playNote(NOTE_C4, 1);
//  indicators_playNote(NOTE_E4, 2);
//  indicators_playNote(NOTE_E4, 2);
//  indicators_playNote(NOTE_G4, 4);
//
//  indicators_playNote(NOTE_REST, 1);
//  indicators_playNote(NOTE_C5, 1);
//  indicators_playNote(NOTE_C5, 1);
//  indicators_playNote(NOTE_C5, 1);
//  indicators_playNote(NOTE_C5, 3);
//  indicators_playNote(NOTE_D5, 1);
//  indicators_playNote(NOTE_A4, 1);
//  indicators_playNote(NOTE_F4, 6);
//}

void indicators_init() {
//  TIM2->CCR1 = 0;
//  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
//  indicators_deepInTheHeartOfTexas();
}

void indicators_periodic(HvcStatus *hvcStatus, VcuOutput *vcuCoreOutput) {
  HAL_GPIO_WritePin(AMS_LED_GPIO_Port, AMS_LED_Pin, (GPIO_PinState) hvcStatus->ams);
  HAL_GPIO_WritePin(IMD_LED_GPIO_Port, IMD_LED_Pin, (GPIO_PinState) hvcStatus->imd);
//  TIM2->CCR1 = vcuCoreOutput->r2dBuzzer ? (TIM2->ARR / 2) : 0;
}