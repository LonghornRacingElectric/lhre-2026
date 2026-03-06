#include "imu.h"

#include <stdint.h>
#include <stdbool.h>

static uint8_t data[6];

static SPI_HandleTypeDef *hspi;

/*private functions =====================================================*/

#define IMU_TIMEOUT 100
static void imu_writeregister1 (uint8_t addr, uint8_t value) {
    HAL_GPIO_WritePin(SPI_CS_IMU_GPIO_Port, SPI_CS_IMU_Pin, GPIO_PIN_RESET);
    data[0] = addr;
    data[1] = value;
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hspi, data, 2, IMU_TIMEOUT);
    if(status != HAL_OK)
        Error_Handler();
    HAL_GPIO_WritePin(SPI_CS_IMU_GPIO_Port, SPI_CS_IMU_Pin, GPIO_PIN_SET);
}

static void imu_readregister(uint8_t size, uint8_t addr) {
    HAL_GPIO_WritePin(SPI_CS_IMU_GPIO_Port, SPI_CS_IMU_Pin, GPIO_PIN_RESET);
    data[0] = addr | 0x80;
    HAL_SPI_Transmit(hspi, data, 1, IMU_TIMEOUT );
    HAL_StatusTypeDef status = HAL_SPI_Receive(hspi, data, size, IMU_TIMEOUT);
    if(status != HAL_OK)
        Error_Handler();
    HAL_GPIO_WritePin(SPI_CS_IMU_GPIO_Port, SPI_CS_IMU_Pin, GPIO_PIN_SET);
}

static void imu_readregister1(uint8_t addr) {
    data[0] = addr;
    imu_readregister(1, addr);
}

static void imu_scale (){

}

/*public functions =======================================================*/
//ACCELEROMETER 208Hz high performance, +/-16g
#define CTRL1_XL_REG 0x10
#define CTRL1_XL_VAL 0b01010100
//GYRO 208Hz high performance, 4000dps
#define CTRL2_G_REG 0x11
#define CTRL2_G_VAL 0b01010001
void imu_init(SPI_HandleTypeDef *hspi_ptr) {
    hspi = hspi_ptr;
    imu_writeregister1(CTRL1_XL_REG, CTRL1_XL_VAL);
    imu_writeregister1(CTRL2_G_REG, CTRL2_G_VAL);
  imu_writeregister1(CTRL1_XL_REG, CTRL1_XL_VAL);
  imu_writeregister1(CTRL2_G_REG, CTRL2_G_VAL);
  imu_writeregister1(CTRL1_XL_REG, CTRL1_XL_VAL);
  imu_writeregister1(CTRL2_G_REG, CTRL2_G_VAL);
    imu_calibrate();
}

void imu_calibrate() {
    // TODO implement
}
#define STATUS_REG 0x1e
bool imu_isAccelReady() {
    imu_readregister1(STATUS_REG);
    bool ready = (data[0] & 0x01);
    return ready;
}

#define OUTX_H_A 0x28
#define ACCEL_LSB 0.00478728f
void imu_getAccel(xyz* vec) {
    imu_readregister(6, OUTX_H_A);
    int16_t accelX = data[0] | (data[1] << 8);
    vec->x = accelX * ACCEL_LSB;
    int16_t accelY = data[2] | (data[3] << 8);
    vec->y = accelY * ACCEL_LSB;
    int16_t accelZ = data[4] | (data[5] << 8);
    vec->z = accelZ * ACCEL_LSB;

}

bool imu_isGyroReady() {
    imu_readregister1(STATUS_REG);
    bool ready = (data[0] & 0x02);
    return ready;
}

#define OUTX_H_G 0x22
#define GYRO_LSB 0.0048869219f // rad/s
void imu_getGyro(xyz* vec) {
    imu_readregister(6, OUTX_H_G);
    int16_t buff_gyroX = data[0] + (data[1] << 8);
    vec->x = buff_gyroX * GYRO_LSB;
    int16_t buff_gyroY = data[2] + (data[3] << 8);
    vec->y = buff_gyroY * GYRO_LSB;
    int16_t buff_gyroZ = data[4] + (data[5] << 8);
    vec->z = buff_gyroZ * GYRO_LSB;
}
