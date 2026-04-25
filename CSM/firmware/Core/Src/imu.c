#include "imu.h"
#include "main.h"
#include "cmsis_os.h"
#include "usbd_cdc_if.h"
#include "spi_mutex.h"

/*******************************************************************************
 * LSM6DSV32XTR Register Definitions
 ******************************************************************************/
#define LSM6DSV32X_WHO_AM_I         0x0F
#define LSM6DSV32X_EXPECTED_ID      0x70

#define LSM6DSV32X_CTRL1_XL         0x10
#define LSM6DSV32X_CTRL2_G          0x11
#define LSM6DSV32X_STATUS_REG       0x1E
#define LSM6DSV32X_OUTX_L_G         0x22
#define LSM6DSV32X_OUTX_L_A         0x28

#define CTRL1_XL_CONFIG    0x54   // 208Hz, ±4g
#define CTRL2_G_CONFIG     0x5C   // 208Hz ±4000dps
#define ACCEL_LSB    0.001197f  // ±4g: 0.122 mg/LSB
#define GYRO_LSB           0.002443f

#define IMU_TIMEOUT             100
#define CALIBRATION_SAMPLES     200



/*******************************************************************************
 * Private Variables
 ******************************************************************************/

static SPI_HandleTypeDef *_hspi;
static uint8_t _buf[6];
static xyz_t _gyro_bias = {0.0f, 0.0f, 0.0f};

/*******************************************************************************
 * Private Functions
 ******************************************************************************/
 static void IMU_PULL_CS_LOW() { // select IMU by pulling CS low
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
}

static void IMU_PULL_CS_HIGH() { // deselect IMU by pulling CS high
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
}


/**
 * @brief  Writes a single byte to an IMU register.
 * @param  reg    Register address
 * @param  val    Value to write
 */
static void imu_write(uint8_t reg, uint8_t val) {
    osMutexAcquire(spi1_mutex, osWaitForever);

    uint8_t tx[2] = { reg & 0x7F, val };
    IMU_PULL_CS_LOW();
    HAL_StatusTypeDef status = HAL_SPI_Transmit(_hspi, tx, 2, IMU_TIMEOUT);
    IMU_PULL_CS_HIGH();

    osMutexRelease(spi1_mutex);
    if (status != HAL_OK) Error_Handler();
}

/**
 * @brief  Reads a single byte from an IMU register.
 * @param  reg    Register address
 * @retval Register value
 */
uint8_t imu_read(uint8_t reg) {
    osMutexAcquire(spi1_mutex, osWaitForever);

    uint8_t transmit = reg | 0x80;
    uint8_t data = 0;
    IMU_PULL_CS_LOW();
    HAL_SPI_Transmit(_hspi, &transmit, 1, IMU_TIMEOUT);
    HAL_SPI_Receive(_hspi, &data, 1, IMU_TIMEOUT);
    IMU_PULL_CS_HIGH();

    osMutexRelease(spi1_mutex);
    return data;
}

/**
 * @brief  Burst reads multiple bytes starting from a register.
 * @param  reg    Starting register address (auto-increments)
 * @param  buf    Buffer to store received bytes
 * @param  len    Number of bytes to read
 */
// static void imu_read_all_axes(uint8_t reg, uint8_t *buf, uint8_t len) {
//     uint8_t tx = reg | 0x80;  // bit7=1 means read
//     IMU_PULL_CS_LOW();
//     HAL_SPI_Transmit(_hspi, &tx, 1, IMU_TIMEOUT);
//     HAL_SPI_Receive(_hspi, buf, len, IMU_TIMEOUT); // len should be 6 for reading X,Y,Z of accel or gyro
//     IMU_PULL_CS_HIGH();
// }
static void imu_read_all_axes(uint8_t reg, uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = imu_read(reg + i);
    }
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

int IMU_Init(SPI_HandleTypeDef *hspi) {
    _hspi = hspi;

    IMU_PULL_CS_HIGH();
    osDelay(10);

    uint8_t id = imu_read(LSM6DSV32X_WHO_AM_I);

    char dbg[64];
    snprintf(dbg, sizeof(dbg), "WHO_AM_I inside init=0x%02X\r\n", id);
    CDC_Transmit_FS((uint8_t*)dbg, strlen(dbg));
    osDelay(200);

    if (id != LSM6DSV32X_EXPECTED_ID) {
        return (int)id == 0 ? -1 : (int)id;
    }

    // Write CTRL1 and CTRL2 only — no CTRL8, no I3C disable, no BDU
    for (int i = 0; i < 3; i++) {
        imu_write(LSM6DSV32X_CTRL1_XL, CTRL1_XL_CONFIG);  // 0x50: 208Hz HP
        imu_write(LSM6DSV32X_CTRL2_G,  CTRL2_G_CONFIG);   // 0x5C: 208Hz ±4000dps
    }

    HAL_Delay(10);
    
    IMU_Calibrate();

    return 0;
}

void IMU_Calibrate(void) {
    // Gyro-only calibration — accel is not calibrated since the car
    // may not be level at startup. Gyro bias is a sensor property
    // independent of orientation.
    xyz_t gyro_sum = {0.0f, 0.0f, 0.0f};
    xyz_t gyro_reading;

    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        while (!IMU_GyroStatus());
        IMU_GetGyro(&gyro_reading);
        gyro_sum.x += gyro_reading.x;
        gyro_sum.y += gyro_reading.y;
        gyro_sum.z += gyro_reading.z;
        osDelay(5);
    }

    _gyro_bias.x = gyro_sum.x / CALIBRATION_SAMPLES;
    _gyro_bias.y = gyro_sum.y / CALIBRATION_SAMPLES;
    _gyro_bias.z = gyro_sum.z / CALIBRATION_SAMPLES;
}

bool IMU_AccelStatus(void) {
    uint8_t status = imu_read(LSM6DSV32X_STATUS_REG);
    return (status & 0x01) != 0;  // bit 0 = XLDA (accelerometer data available)
}

bool IMU_GyroStatus(void) {
    uint8_t status = imu_read(LSM6DSV32X_STATUS_REG);
    return (status & 0x02) != 0;  // bit 1 = GDA (gyroscope data available)
}

void IMU_GetAccel(xyz_t *vec) {
    imu_read_all_axes(LSM6DSV32X_OUTX_L_A, _buf, 6);

    int16_t raw_x = (int16_t)((uint16_t)_buf[0] | ((uint16_t)_buf[1] << 8));
    int16_t raw_y = (int16_t)((uint16_t)_buf[2] | ((uint16_t)_buf[3] << 8));
    int16_t raw_z = (int16_t)((uint16_t)_buf[4] | ((uint16_t)_buf[5] << 8));

    vec->x = raw_x * ACCEL_LSB;
    vec->y = raw_y * ACCEL_LSB;
    vec->z = raw_z * ACCEL_LSB;
}

void IMU_GetGyro(xyz_t *vec) {
    imu_read_all_axes(LSM6DSV32X_OUTX_L_G, _buf, 6);

    // Each axis is two bytes, little-endian (low byte first)
    int16_t raw_x = (int16_t)((uint16_t)_buf[0] | ((uint16_t)_buf[1] << 8));
    int16_t raw_y = (int16_t)((uint16_t)_buf[2] | ((uint16_t)_buf[3] << 8));
    int16_t raw_z = (int16_t)((uint16_t)_buf[4] | ((uint16_t)_buf[5] << 8));

    // Convert raw counts to rad/s, then subtract gyro bias
    vec->x = (raw_x * GYRO_LSB) - _gyro_bias.x;
    vec->y = (raw_y * GYRO_LSB) - _gyro_bias.y;
    vec->z = (raw_z * GYRO_LSB) - _gyro_bias.z;
}

void IMU_GetData(imu_data_t *out) {
    while (!IMU_AccelStatus());
    while (!IMU_GyroStatus());
    IMU_GetAccel(&out->accel);
    IMU_GetGyro(&out->gyro);
}