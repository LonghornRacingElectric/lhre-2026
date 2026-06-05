#include "imu.h"
#include "main.h"
#include <string.h>

/* ── ASM330LHBTR Register Map ── */
#define ASM330_WHO_AM_I     0x0F
#define ASM330_EXPECTED_ID  0x70

#define ASM330_CTRL1_XL     0x10   // accel control
#define ASM330_CTRL2_G      0x11   // gyro control
#define ASM330_OUTX_L_A     0x28   // first accel output register

/* CTRL1_XL: ODR=104Hz (0100), FS=±4g (10), LPF2 off → 0x48 */
#define CTRL1_XL_CONFIG     0x48

/* CS pin */
#define IMU_CS_PORT  GPIOB
#define IMU_CS_PIN   GPIO_PIN_10

#define IMU_CS_LOW()   HAL_GPIO_WritePin(IMU_CS_PORT, IMU_CS_PIN, GPIO_PIN_RESET)
#define IMU_CS_HIGH()  HAL_GPIO_WritePin(IMU_CS_PORT, IMU_CS_PIN, GPIO_PIN_SET)

/* ±4g, 16-bit: sensitivity = 0.122 mg/LSB = 0.001197 m/s^2/LSB */
#define ACCEL_SCALE  0.001197f

static SPI_HandleTypeDef *_hspi;

/* ── Internal helpers ── */

static void imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg & 0x7F, val };
    IMU_CS_LOW();
    HAL_SPI_Transmit(_hspi, tx, 2, 10);
    IMU_CS_HIGH();
}

static uint8_t imu_read_reg(uint8_t reg)
{
    uint8_t tx = 0x80 | reg;
    uint8_t rx = 0;
    IMU_CS_LOW();
    HAL_SPI_Transmit(_hspi, &tx, 1, 10);
    HAL_SPI_Receive(_hspi, &rx, 1, 10);
    IMU_CS_HIGH();
    return rx;
}

static void imu_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t tx = 0x80 | reg;
    IMU_CS_LOW();
    HAL_SPI_Transmit(_hspi, &tx, 1, 10);
    HAL_SPI_Receive(_hspi, buf, len, 20);
    IMU_CS_HIGH();
}

/* ── Public API ── */

int IMU_Init(SPI_HandleTypeDef *hspi)
{
    _hspi = hspi;

    IMU_CS_HIGH();
    HAL_Delay(10);

    uint8_t id = imu_read_reg(ASM330_WHO_AM_I);
    if (id != ASM330_EXPECTED_ID) {
        return (int)id;  // returns raw ID so caller can print it for debug
    }

    imu_write_reg(ASM330_CTRL2_G,  0x00);  // gyro off
    imu_write_reg(ASM330_CTRL1_XL, CTRL1_XL_CONFIG);

    HAL_Delay(10);
    return 0;
}

int IMU_Read(imu_data_t *out)
{
    uint8_t buf[6];
    imu_read_burst(ASM330_OUTX_L_A, buf, 6);

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    out->accel_x = raw_x * ACCEL_SCALE;
    out->accel_y = raw_y * ACCEL_SCALE;
    out->accel_z = raw_z * ACCEL_SCALE;

    return 0;
}
