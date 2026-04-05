/**
 * @file adbms.cpp
 * @author Pranit Arya
 * @brief Communication functions for ADBMScheck IC (LTC6813)
 * @version 0.1
 * @date 2023-10-29
 *
 * Created by Pranit Arya 10/29/23
 *
 */

#include <cstdint>
#include "adbms.h"
#include "spi.h"


// Interrupt callbacks
static ADBMS6830_Error_t bms_error = ADBMS6830_ERROR;
uint16_t command_counter = 0;

uint8_t cfga[NUM_BMS_ICS * 6];
uint8_t cfgb[NUM_BMS_ICS * 6];

uint8_t cfga_default[6] = {
  0x81, // REFON = 1, Default for CTH
  0x00,
  0x00,
  // 0xFE, // Disabling Pulldown for GPIO 2-8, turn on GPIO 1 (LED)
  0xFF, // Disabling Pulldown for GPIO 1-8 (no LED)
  0x03, // Disabling Pulldown for GPIO 9-10
  0x01, // IIR filter corner frequency 110 Hz
};
uint8_t cfgb_default[6] = {
  0x00,
  0xF8,
  0x7F,
  0x00,
  0x00, // DCC
  0x00, // DCC
};

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  bms_error = ADBMS6830_OK;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
  bms_error = ADBMS6830_SPI_ERROR;
}

ADBMS6830_Error_t adbms6830_cmd_write(ADBMS6830_Command_t command, uint8_t *data_buf) {
  uint16_t command_int = command;
  ADBMS6830_Error_t output_status = ADBMS6830_OK;

  uint8_t cmd_buf[2];
  cmd_buf[0] = command_int >> 8;
  cmd_buf[1] = command_int & 0xFF;
  uint16_t cmd_crc = pec(cmd_buf, 2);
  uint8_t crc_buf[2];
  crc_buf[0] = cmd_crc >> 8;
  crc_buf[1] = cmd_crc & 0xFF;

  HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_RESET);

  // Send command
  if (HAL_SPI_Transmit(&hspi2, cmd_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
    return ADBMS6830_SPI_ERROR;
  }
  if (HAL_SPI_Transmit(&hspi2, crc_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
    return ADBMS6830_SPI_ERROR;
  }

  for(int i = 0; i < NUM_BMS_ICS; i++) {
    uint16_t data_crc = dpec(data_buf + (i * 6), 6, true, 0);
    crc_buf[0] = data_crc >> 8;
    crc_buf[1] = data_crc & 0xFF;

    if (HAL_SPI_Transmit(&hspi2, data_buf + (i * 6), 6, LTC_SPI_TIMEOUT) != HAL_OK) {
      HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
      return ADBMS6830_SPI_ERROR;
    }
    if (HAL_SPI_Transmit(&hspi2, crc_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
      HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
      return ADBMS6830_SPI_ERROR;
    }
  }

  HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);

  return ADBMS6830_OK;
}

uint32_t adbms6830_cmd_read(ADBMS6830_Command_t command, uint8_t *data_buf) {
//  adbms6830_wakeup();
  uint16_t command_int = command;

  uint8_t cmd_buf[2];
  cmd_buf[0] = command_int >> 8;
  cmd_buf[1] = command_int & 0xFF;
  uint16_t cmd_crc = pec(cmd_buf, 2);
  uint8_t crc_buf[2];
  crc_buf[0] = cmd_crc >> 8;
  crc_buf[1] = cmd_crc & 0xFF;

  HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_RESET);

  // Send command
  if (HAL_SPI_Transmit(&hspi2, cmd_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
    HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
    return 0;
  }
  if (HAL_SPI_Transmit(&hspi2, crc_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
    HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
    return 0;
  }

  // Get result from daisy chain, check CRC
  uint32_t responsiveChips = 0;
  for (int i = 0; i < NUM_BMS_ICS; i++) {
    if (HAL_SPI_Receive(&hspi2, data_buf + (i * 6), 6, LTC_SPI_TIMEOUT) != HAL_OK) {
      HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
      return 0;
    }
    if (HAL_SPI_Receive(&hspi2, crc_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
      HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
      return 0;
    }
    volatile uint16_t received_data_crc = ((crc_buf[0] << 8) | crc_buf[1]);
    uint16_t possible_command_counter = received_data_crc >> 10;
    received_data_crc = received_data_crc & 0x3FF;
    volatile uint16_t data_crc = dpec(data_buf + (i * 6), 6, true, possible_command_counter);
    if (data_crc == received_data_crc)
    {
      // command_counter = possible_command_counter;
      responsiveChips++;
    }
  }

  HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);

  return responsiveChips;

}


uint32_t adbms6830_cmd_poll(ADBMS6830_Command_t command) {
  volatile uint16_t command_int = command;

  uint8_t cmd_buf[2];
  cmd_buf[0] = command_int >> 8;
  cmd_buf[1] = command_int & 0xFF;
  uint16_t cmd_crc = pec(cmd_buf, 2);
  uint8_t crc_buf[2];
  crc_buf[0] = cmd_crc >> 8;
  crc_buf[1] = cmd_crc & 0xFF;
//  adbms6830_wakeup();

  HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_RESET);

  // Send command
  if (HAL_SPI_Transmit(&hspi2, cmd_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
    HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
    return 0;
  }
  if (HAL_SPI_Transmit(&hspi2, crc_buf, 2, LTC_SPI_TIMEOUT) != HAL_OK) {
    HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
    return 0;
  }

  // TODO ignoring poll for now
  // uint8_t poll_buf = 0;
  // volatile int i;
  // for(i = 0; i < 500; i++) {
  //   HAL_SPI_Receive(&hspi2, &poll_buf, 1, LTC_SPI_TIMEOUT);
  //   if(poll_buf) break;
  // }
  HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);

  return 1;
}

uint32_t adbms6830_adcv() {
//  adbms6830_wakeup();
  return adbms6830_cmd_poll(CMD_ADCV);
}

uint32_t adbms6830_adax() {
//  adbms6830_wakeup();
  return adbms6830_cmd_poll(CMD_ADAX_ALL);
}

uint32_t adbms6830_adstat() {
  return adbms6830_cmd_poll(CMD_ADSTAT_ALL);
}

void ltc6813_adowUp() {
  for(int i = 0; i < 50; i++) {
      adbms6830_cmd_poll(CMD_ADOW_PUP);
  }
}
void ltc6813_adowDown() {
  for(int i = 0; i < 50; i++) {
      adbms6830_cmd_poll(CMD_ADOW_PDOWN);
  }
}

void adbms6830_wrcfga() {
  for(int i = 0; i < NUM_BMS_ICS; i++)
  {
    for(int j = 0; j < 6; j++)
    {
      cfga[6*i + j] = cfga_default[j];
    }
  }
  adbms6830_cmd_write(CMD_WRCFGA, cfga);
}

void adbms6830_wrcfgb(bool enableBalancing, const bool balanceCommands[NUM_BMS_ICS*14]) {
  for(int i = 0; i < NUM_BMS_ICS; i++)
  {
    for(int j = 0; j < 6; j++)
    {
      cfgb[6*i + j] = cfgb_default[j];
    }

    uint16_t dcc = 0;
    if(enableBalancing)
    {
      for(int k = 0; k < 14; k++)
      {
        dcc |= balanceCommands[14*(NUM_BMS_ICS-i-1) + k] << k;
      }
      cfgb[6*i + 4] = dcc & 0xFF;
      cfgb[6*i + 5] = dcc >> 8;
    }
  }
  adbms6830_cmd_write(CMD_WRCFGB, cfgb);
}

void adbms6830_wakeup() {
  uint8_t x = 0xFF;
  for (int i = 0; i < NUM_BMS_ICS; i++) {
    HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2, &x, 1, 100);
    HAL_GPIO_WritePin(SPI_CS_BMS_GPIO_Port, SPI_CS_BMS_Pin, GPIO_PIN_SET);
    for(volatile int j = 0; j < 100; j++);
  }
}

/************************************************
 * ADI-given pec Code
*************************************************/

/************************************
Copyright 2012 Analog Devices, Inc. (ADI)
Permission to freely use, copy, modify, and distribute this software for any purpose with or
without fee is hereby granted, provided that the above copyright notice and this permission
notice appear in all copies: THIS SOFTWARE IS PROVIDED “AS IS” AND ADI DISCLAIMS ALL WARRANTIES
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL ADI BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM ANY
USE OF SAME, INCLUDING ANY LOSS OF USE OR DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTUOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
***************************************/

/**
 * @brief Generate pec CRC for ADBMS6830
 *
 * @param data
 * @param len
 * @return uint16_t
 */
static uint16_t pec(const uint8_t *data, int len) {
  uint16_t remainder, address;
  remainder = 16; // PEC seed
  for (int i = 0; i < len; i++) {
    address = ((remainder >> 7) ^ data[i]) & 0xff; //calculate PEC table address
    remainder = (remainder << 8) ^ pec15Table[address];
  }
  return (remainder * 2);//The CRC15 has a 0 in the LSB so the final value must be multiplied by 2
}

// from Analog Devices
// static uint16_t dpec(const uint8_t *data, int len, bool rx_cmd, uint8_t cc)
// {
//   uint16_t remainder = 16; /* PEC_SEED;   0000010000 */
//   uint16_t polynom = 0x8F; /* x10 + x7 + x3 + x2 + x + 1 <- the CRC15 polynomial         100 1000 1111   48F */
//
//   /* Perform modulo-2 division, a byte at a time. */
//   for (uint8_t pbyte = 0; pbyte < len; ++pbyte)
//   {
//     /* Bring the next byte into the remainder. */
//     remainder ^= (uint16_t)(data[pbyte] << 2);
//     /* Perform modulo-2 division, a bit at a time.*/
//     for (uint8_t bit_ = 8; bit_ > 0; --bit_)
//     {
//       /* Try to divide the current data bit. */
//       if ((remainder & 0x200) > 0)//equivalent to remainder & 2^14 simply check for MSB
//       {
//         remainder = (uint16_t)((remainder << 1));
//         remainder = (uint16_t)(remainder ^ polynom);
//       }
//       else
//       {
//         remainder = (uint16_t)(remainder << 1);
//       }
//     }
//   }
//   if (rx_cmd == true)
//   {
//     remainder ^= (uint16_t)(((cc << 2) & 0xFC) << 2);
//     /* Perform modulo-2 division, a bit at a time */
//     for (uint8_t bit_ = 6; bit_ > 0; --bit_)
//     {
//       /* Try to divide the current data bit */
//       if ((remainder & 0x200) > 0)//equivalent to remainder & 2^14 simply check for MSB
//       {
//         remainder = (uint16_t)((remainder << 1));
//         remainder = (uint16_t)(remainder ^ polynom);
//       }
//       else
//       {
//         remainder = (uint16_t)((remainder << 1));
//       }
//     }
//   }
//   return ((uint16_t)(remainder & 0x3FF));
// }


static uint16_t dpec(const uint8_t *data, int len, bool rx_cmd, uint8_t cc)
{
  uint16_t remainder = 16;
  uint16_t polynomial = 0x8F;

  for (int i = 0; i < len; i++) {
    uint16_t address = ((remainder >> 2) ^ data[i]) & 0xFF;
    remainder = (remainder << 8) ^ pec10Table[address];
  }

  if(rx_cmd)
  {
    remainder ^= ((cc << 2) & 0xFC) << 2;

    for (uint8_t bit_ = 6; bit_ > 0; --bit_)
    {
      if ((remainder & 0x200) != 0)
      {
        remainder = remainder << 1;
        remainder = remainder ^ polynomial;
      }
      else
      {
        remainder = remainder << 1;
      }
    }

  }
  remainder = remainder & 0x3FF;

  return remainder;
}
