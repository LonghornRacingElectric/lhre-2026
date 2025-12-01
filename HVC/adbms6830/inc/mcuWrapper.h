/*******************************************************************************
Copyright (c) 2020 - Analog Devices Inc. All Rights Reserved.
This software is proprietary & confidential to Analog Devices, Inc.
and its licensor.
******************************************************************************
* @file:    mcuWrapper.h
* @brief:   Generic wrapper header file
* @version: $Revision$
* @date:    $Date$
* Developed by: ADIBMS Software team, Bangalore, India
*****************************************************************************/
/** @addtogroup MUC_DRIVER
*  @{
*
*/

/** @addtogroup SPI_DRIVER SPI DRIVER
*  @{
*
*/
#ifndef __ADBMSWRAPPER_H
#define __ADBMSWRAPPER_H
#include "common.h"

#ifdef MBED

#else
// HVC will implement these functions - see HVC/firmware/Core/Src/hvc_bms.c
// #include "main.h"
#include "stm32g4xx_hal.h"
// #include "stm32g4xx_it.h"

// HVC hardware configuration - will be defined in hvc_bms.c
extern SPI_HandleTypeDef hspi4;         /* HVC uses SPI4 */

#define CS_PIN GPIO_PIN_3               /* HVC CS on PE3 */
#define GPIO_PORT GPIOE                 /* HVC CS port */
#endif

void Delay_ms(uint32_t delay);
void adBmsCsLow(void);
void adBmsCsHigh(void);
void spiWriteBytes
( 
  uint16_t size,                                /*Option: Number of bytes to be written on the SPI port*/
  uint8_t *tx_Data                              /*Array of bytes to be written on the SPI port*/
);
void spiWriteReadBytes
(
  uint8_t *tx_data,                             /*array of data to be written on SPI port*/
  uint8_t *rx_data,                             /*Input: array that will store the data read by the SPI port*/
  uint16_t size                             /*Option: number of bytes*/
);
void spiReadBytes(uint16_t size, uint8_t *rx_data);
void startTimer(void);
void stopTimer(void);
uint32_t getTimCount(void);
void adBmsWakeupIc(uint8_t total_ic);

#endif
/** @}*/
/** @}*/
