/**
 * @brief Header for ADBMS
 * @author Pranit Arya
 * @date 10-29-23
 *
 */

#include <cstdint>

#ifndef ADBMS_H
#define ADBMS_H

#define LTC_SPI_TIMEOUT 1000 // Timeout
#define NUM_BMS_ICS 10 // Number of BMS ICs in daisy chain

// Not used here - just for reference
#define CRC15_POLY 0x4599;

#define MD ADC_MODE_NORMAL // ADC Mode
#define DCP DISCHARGE_PERMITTED // Discharge Permit
#define CH_ALL CELL_SELECTION_ALL // Cell Channels
#define CH_1 CELL_SELECTION_1_7_13 // Cell Channels
#define CH_2 CELL_SELECTION_2_8_14 // Cell Channels
#define CH_3 CELL_SELECTION_3_9_15 // Cell Channels
#define CH_4 CELL_SELECTION_4_10_16 // Cell Channels
#define CH_5 CELL_SELECTION_5_11_17 // Cell Channels
#define CH_6 CELL_SELECTION_6_12_18 // Cell Channels

extern uint16_t command_counter; // command counter state

typedef enum {
    ADBMS6830_OK,
    ADBMS6830_SPI_ERROR,
    LTC6813_INVALID_DATA, // CRC mismatch
    ADBMS6830_ERROR //Todo: More specific
} ADBMS6830_Error_t;

typedef enum {
    ADC_MODE_SLOW = 0x0, // 422Hz
    ADC_MODE_FAST = 0x1, // 27kHz
    ADC_MODE_NORMAL = 0x2, // 7kHz
    ADC_MODE_FILTERED = 0x3 // 26Hz
} LTC6813_ADC_Mode_t;

typedef enum {
    DISCHARGE_NOT_PERMITTED = 0x0,
    DISCHARGE_PERMITTED = 0x1
} LTC6813_Discharge_t;

typedef enum {
    CELL_SELECTION_ALL = 0x0,
    CELL_SELECTION_1_7_13 = 0x1,
    CELL_SELECTION_2_8_14 = 0x2,
    CELL_SELECTION_3_9_15 = 0x3,
    CELL_SELECTION_4_10_16 = 0x4,
    CELL_SELECTION_5_11_17 = 0x5,
    CELL_SELECTION_6_12_18 = 0x6
} LTC6813_ADC_Cell_Selection_t;

typedef enum {
    PUP_PULLDOWN = 0x0,
    PUP_PULLUP = 0x1
} LTC6813_Pull_Up_Pull_Down_t;

typedef enum {
    GPIO_1_5_6_10 = 0x0,
    GPIO_1_6 = 0x1,
    GPIO_2_7 = 0x2,
    GPIO_3_8 = 0x3,
    GPIO_4_9 = 0x4,
    GPIO_5 = 0x5,
    GPIO_2ND_REFERENCE = 0x6
} LTC6813_ADC_GPIO_Selection_t;

typedef enum {
    STATUS_GROUP_ALL = 0x0,
    STATUS_GROUP_SC = 0x1,
    STATUS_GROUP_ITMP = 0x2,
    STATUS_GROUP_VA = 0x3,
    STATUS_GROUP_VD = 0x4
} LTC6813_Status_Group_Selection_t;

typedef struct {
    int adc_number;
    uint16_t cell_voltages[18];
    uint16_t status[4];
    uint16_t temperature;
    uint16_t analog_gpio[5];
} LTC6813_Data_t;

// From UBC team's public Github code
typedef enum {
    CMD_WRCFGA  = 0x0001,       // Write Configuration Register Group A, Fixed
    CMD_WRCFGB  = 0x0024,       // Write Configuration Register Group B
    CMD_RDCFGA  = 0x0002,       // Read Configuration Register Group A
    CMD_RDCFGB  = 0x0026,       // Read Configuration Register Group B
    CMD_RDCVA   = 0x0004,       // Read Cell Voltage Register Group A, Fixed
    CMD_RDCVB   = 0x0006,       // Read Cell Voltage Register Group B, Fixed
    CMD_RDCVC   = 0x0008,       // Read Cell Voltage Register Group C, Fixed
    CMD_RDCVD   = 0x000A,       // Read Cell Voltage Register Group D, Fixed
    CMD_RDCVE   = 0x0009,       // Read Cell Voltage Register Group E, Fixed
    CMD_RDCVF   = 0x000B,       // Read Cell Voltage Register Group F, Fixed
    CMD_RDFCA   = 0x0012,       // Read Filtered Cell Voltage Register Group A, Fixed
    CMD_RDFCB   = 0x0013,       // Read Filtered Cell Voltage Register Group B, Fixed
    CMD_RDFCC   = 0x0014,       // Read Filtered Cell Voltage Register Group C, Fixed
    CMD_RDFCD   = 0x0015,       // Read Filtered Cell Voltage Register Group D, Fixed
    CMD_RDFCE   = 0x0016,       // Read Filtered Cell Voltage Register Group E, Fixed
    CMD_RDFCF   = 0x0007,       // Read Filtered Cell Voltage Register Group F, Fixed
    CMD_RDAUXA  = 0x0019,       // Read Auxilliary Register Group A, Fixed
    CMD_RDAUXB  = 0x001A,       // Read Auxilliary Register Group B, Fixed
    CMD_RDAUXC  = 0x001B,       // Read Auxilliary Register Group C, Fixed
    CMD_RDAUXD  = 0x001F,       // Read Auxilliary Register Group D, Fixed
    CMD_RDSTATA = 0x0030,       // Read Status Register Group A, Fixed
    CMD_RDSTATB = 0x0031,       // Read Status Register Group B, Fixed
    CMD_WRSCTRL = 0x0014,       // Write S Control Register Group
    CMD_WRPWM   = 0x0020,       // Write PWM Register Group
    CMD_WRPSB   = 0x001C,       // Write PWM/S Control Register Group B
    CMD_RDSCTRL = 0x0016,       // Read S Control Register Group
    CMD_RDPWM   = 0x0022,       // Read PWM Register Group
    CMD_RDPSB   = 0x001E,       // Read PWM/S Control Register Group B
    CMD_STSCTRL = 0x0019,       // Start S Control Pulsing and Poll Status
    CMD_CLRSCTRL= 0x0018,       // Clear S Control Register Group
    CMD_WRPWMA  = 0x0020,       // Write PWM Register Group A
    CMD_RDPWMA  = 0x0022,       // Read PWM Register Group A
    CMD_WRPWMB  = 0x0021,       // Write PWM Register Group B
    CMD_RDPWMB  = 0x0023,       // Read PWM Register Group B

    // Start Cell Voltage ADC Conversion and Poll Status
    CMD_ADCV    = 0x0260,
//    CMD_ADCV     = 0x0260 | (MD << 7) | (DCP << 4) | CH_ALL,
//    CMD_ADCV_CH1 = 0x0260 | (MD << 7) | (DCP << 4) | CH_1,
//    CMD_ADCV_CH2 = 0x0260 | (MD << 7) | (DCP << 4) | CH_2,
//    CMD_ADCV_CH3 = 0x0260 | (MD << 7) | (DCP << 4) | CH_3,
//    CMD_ADCV_CH4 = 0x0260 | (MD << 7) | (DCP << 4) | CH_4,
//    CMD_ADCV_CH5 = 0x0260 | (MD << 7) | (DCP << 4) | CH_5,
//    CMD_ADCV_CH6 = 0x0260 | (MD << 7) | (DCP << 4) | CH_6,

    // Start Open Wire ADC Conversion and Poll Status TODO: fix codes
    CMD_ADOW_PUP    = 0x0228 | (MD << 7) | (PUP_PULLUP << 6)   | (DCP << 4), // CH set to 0
    CMD_ADOW_PDOWN  = 0x0228 | (MD << 7) | (PUP_PULLDOWN << 6) | (DCP << 4), // CH set to 0

    // Start Self Test Cell Voltage Conversion and Poll Status
    //CMD_CVST    = 0x0207 | (MD << 7) | (ST << 5),

    // Start Overlap Measurement of Cell 7 Voltage
    CMD_ADOL    = 0x0201 | (MD << 7) | (DCP << 4),

    // Start GPIOs ADC Conversion and Poll Status
    CMD_ADAX_ALL    = 0x0410,   // ALL Channels, Fixed
//    CMD_ADAX_ALL      =  0x0460 | (MD << 7) | GPIO_1_5_6_10,
//    CMD_ADAX_GPIO1_6  =  0x0460 | (MD << 7) | GPIO_1_6,
//    CMD_ADAX_GPIO2_7  =  0x0460 | (MD << 7) | GPIO_2_7,
//    CMD_ADAX_GPIO3_8  =  0x0460 | (MD << 7) | GPIO_3_8,
//    CMD_ADAX_GPIO4_9  =  0x0460 | (MD << 7) | GPIO_4_9,
//    CMD_ADAX_GPIO5    =  0x0460 | (MD << 7) | GPIO_5,
//    CMD_ADAX_VREF2    =  0x0460 | (MD << 7) | GPIO_2ND_REFERENCE,
//    // Start GPIOs ADC Conversion With Digital Redundancy and Poll Status
//    CMD_ADAXD_ALL     =  0x0400 | (MD << 7) | GPIO_1_5_6_10,
//    CMD_ADAXD_GPIO1_6 =  0x0400 | (MD << 7) | GPIO_1_6,
//    CMD_ADAXD_GPIO2_7 =  0x0400 | (MD << 7) | GPIO_2_7,
//    CMD_ADAXD_GPIO3_8 =  0x0400 | (MD << 7) | GPIO_3_8,
//    CMD_ADAXD_GPIO4_9 =  0x0400 | (MD << 7) | GPIO_4_9,
//    CMD_ADAXD_GPIO5   =  0x0400 | (MD << 7) | GPIO_5,
//    CMD_ADAXD_VREF2   =  0x0400 | (MD << 7) | GPIO_2ND_REFERENCE,

    // Start Self Test GPIOs Conversion and Poll Status
    //CMD_AXST = 0x0407 | (MD << 7) | (ST << 5),

    // Start Status Group ADC Conversion and Poll Status
    CMD_ADSTAT_ALL  = 0x0468 | (MD << 7) | STATUS_GROUP_ALL, // NOT Found in ADBMS6830
//    CMD_ADSTAT_ALL  = 0x0468 | (MD << 7) | STATUS_GROUP_ALL,
//    CMD_ADSTAT_SC   = 0x0468 | (MD << 7) | STATUS_GROUP_SC,
//    CMD_ADSTAT_ITMP = 0x0468 | (MD << 7) | STATUS_GROUP_ITMP,
//    CMD_ADSTAT_VA   = 0x0468 | (MD << 7) | STATUS_GROUP_VA,
//    CMD_ADSTAT_VD   = 0x0468 | (MD << 7) | STATUS_GROUP_VD,
//    // Start Status Group ADC Conversion With Digital Redundancy and Poll Status
//    CMD_ADSTATD_ALL = 0x0408 | (MD << 7) | STATUS_GROUP_ALL,
//    CMD_ADSTATD_SC  = 0x0408 | (MD << 7) | STATUS_GROUP_SC,
//    CMD_ADSTATD_ITMP= 0x0408 | (MD << 7) | STATUS_GROUP_ITMP,
//    CMD_ADSTATD_VA  = 0x0408 | (MD << 7) | STATUS_GROUP_VA,
//    CMD_ADSTATD_VD  = 0x0408 | (MD << 7) | STATUS_GROUP_VD,

    // Start Self Test Status Group Conversion and Poll Status
    //CMD_STATST  = 0x040F | (MD << 7) | (ST << 5),

    // Start Combined Cell Voltage and GPIO1, GPIO2 Conversion and Poll Status
    CMD_ADCVAX  = 0x046F | (MD << 7) | (DCP << 4),
    // Start Combined Cell Voltage and SC Conversion and Poll Status
    CMD_ADCVSC  = 0x0467 | (MD << 7) | (DCP << 4),

    CMD_CLRCELL = 0x0711,       // Clear Cell Voltage Register Groups
    CMD_CLRAUX  = 0x0712,       // Clear Auxiliary Register Groups
    CMD_CLRSTAT = 0x0713,       // Clear Status Register Groups
    CMD_PLADC   = 0x0714,       // Poll ADC Conversion Status
    CMD_DIAGN   = 0x0715,       // Diagnose MUX and Poll Status
    CMD_WRCOMM  = 0x0721,       // Write COMM Register Group
    CMD_RDCOMM  = 0x0722,       // Read COMM Register Group
    CMD_STCOMM  = 0x0723,       // Start I2C /SPI Communication
    CMD_MUTE    = 0x0028,       // Mute discharge
    CMD_UNMUTE  = 0x0029,        // Unmute discharge

    CMD_RDSID = 0x002C,  // read serial number
} ADBMS6830_Command_t;

// Pre-generated PEC15 table by running code given in ADI datsheet
const uint16_t pec15Table[256] {0x0,0xc599, 0xceab, 0xb32, 0xd8cf, 0x1d56, 0x1664, 0xd3fd, 0xf407, 0x319e, 0x3aac,  //!<precomputed CRC15 Table
                                0xff35, 0x2cc8, 0xe951, 0xe263, 0x27fa, 0xad97, 0x680e, 0x633c, 0xa6a5, 0x7558, 0xb0c1,
                                0xbbf3, 0x7e6a, 0x5990, 0x9c09, 0x973b, 0x52a2, 0x815f, 0x44c6, 0x4ff4, 0x8a6d, 0x5b2e,
                                0x9eb7, 0x9585, 0x501c, 0x83e1, 0x4678, 0x4d4a, 0x88d3, 0xaf29, 0x6ab0, 0x6182, 0xa41b,
                                0x77e6, 0xb27f, 0xb94d, 0x7cd4, 0xf6b9, 0x3320, 0x3812, 0xfd8b, 0x2e76, 0xebef, 0xe0dd,
                                0x2544, 0x2be, 0xc727, 0xcc15, 0x98c, 0xda71, 0x1fe8, 0x14da, 0xd143, 0xf3c5, 0x365c,
                                0x3d6e, 0xf8f7,0x2b0a, 0xee93, 0xe5a1, 0x2038, 0x7c2, 0xc25b, 0xc969, 0xcf0, 0xdf0d,
                                0x1a94, 0x11a6, 0xd43f, 0x5e52, 0x9bcb, 0x90f9, 0x5560, 0x869d, 0x4304, 0x4836, 0x8daf,
                                0xaa55, 0x6fcc, 0x64fe, 0xa167, 0x729a, 0xb703, 0xbc31, 0x79a8, 0xa8eb, 0x6d72, 0x6640,
                                0xa3d9, 0x7024, 0xb5bd, 0xbe8f, 0x7b16, 0x5cec, 0x9975, 0x9247, 0x57de, 0x8423, 0x41ba,
                                0x4a88, 0x8f11, 0x57c, 0xc0e5, 0xcbd7, 0xe4e, 0xddb3, 0x182a, 0x1318, 0xd681, 0xf17b,
                                0x34e2, 0x3fd0, 0xfa49, 0x29b4, 0xec2d, 0xe71f, 0x2286, 0xa213, 0x678a, 0x6cb8, 0xa921,
                                0x7adc, 0xbf45, 0xb477, 0x71ee, 0x5614, 0x938d, 0x98bf, 0x5d26, 0x8edb, 0x4b42, 0x4070,
                                0x85e9, 0xf84, 0xca1d, 0xc12f, 0x4b6, 0xd74b, 0x12d2, 0x19e0, 0xdc79, 0xfb83, 0x3e1a, 0x3528,
                                0xf0b1, 0x234c, 0xe6d5, 0xede7, 0x287e, 0xf93d, 0x3ca4, 0x3796, 0xf20f, 0x21f2, 0xe46b, 0xef59,
                                0x2ac0, 0xd3a, 0xc8a3, 0xc391, 0x608, 0xd5f5, 0x106c, 0x1b5e, 0xdec7, 0x54aa, 0x9133, 0x9a01,
                                0x5f98, 0x8c65, 0x49fc, 0x42ce, 0x8757, 0xa0ad, 0x6534, 0x6e06, 0xab9f, 0x7862, 0xbdfb, 0xb6c9,
                                0x7350, 0x51d6, 0x944f, 0x9f7d, 0x5ae4, 0x8919, 0x4c80, 0x47b2, 0x822b, 0xa5d1, 0x6048, 0x6b7a,
                                0xaee3, 0x7d1e, 0xb887, 0xb3b5, 0x762c, 0xfc41, 0x39d8, 0x32ea, 0xf773, 0x248e, 0xe117, 0xea25,
                                0x2fbc, 0x846, 0xcddf, 0xc6ed, 0x374, 0xd089, 0x1510, 0x1e22, 0xdbbb, 0xaf8, 0xcf61, 0xc453,
                                0x1ca, 0xd237, 0x17ae, 0x1c9c, 0xd905, 0xfeff, 0x3b66, 0x3054, 0xf5cd, 0x2630, 0xe3a9, 0xe89b,
                                0x2d02, 0xa76f, 0x62f6, 0x69c4, 0xac5d, 0x7fa0, 0xba39, 0xb10b, 0x7492, 0x5368, 0x96f1, 0x9dc3,
                                0x585a, 0x8ba7, 0x4e3e, 0x450c, 0x8095
};

const uint16_t pec10Table[256] = {
        0x000, 0x08f, 0x11e, 0x191, 0x23c, 0x2b3, 0x322, 0x3ad, 0x0f7, 0x078, 0x1e9, 0x166, 0x2cb, 0x244, 0x3d5, 0x35a,
        0x1ee, 0x161, 0x0f0, 0x07f, 0x3d2, 0x35d, 0x2cc, 0x243, 0x119, 0x196, 0x007, 0x088, 0x325, 0x3aa, 0x23b, 0x2b4,
        0x3dc, 0x353, 0x2c2, 0x24d, 0x1e0, 0x16f, 0x0fe, 0x071, 0x32b, 0x3a4, 0x235, 0x2ba, 0x117, 0x198, 0x009, 0x086,
        0x232, 0x2bd, 0x32c, 0x3a3, 0x00e, 0x081, 0x110, 0x19f, 0x2c5, 0x24a, 0x3db, 0x354, 0x0f9, 0x076, 0x1e7, 0x168,
        0x337, 0x3b8, 0x229, 0x2a6, 0x10b, 0x184, 0x015, 0x09a, 0x3c0, 0x34f, 0x2de, 0x251, 0x1fc, 0x173, 0x0e2, 0x06d,
        0x2d9, 0x256, 0x3c7, 0x348, 0x0e5, 0x06a, 0x1fb, 0x174, 0x22e, 0x2a1, 0x330, 0x3bf, 0x012, 0x09d, 0x10c, 0x183,
        0x0eb, 0x064, 0x1f5, 0x17a, 0x2d7, 0x258, 0x3c9, 0x346, 0x01c, 0x093, 0x102, 0x18d, 0x220, 0x2af, 0x33e, 0x3b1,
        0x105, 0x18a, 0x01b, 0x094, 0x339, 0x3b6, 0x227, 0x2a8, 0x1f2, 0x17d, 0x0ec, 0x063, 0x3ce, 0x341, 0x2d0, 0x25f,
        0x2e1, 0x26e, 0x3ff, 0x370, 0x0dd, 0x052, 0x1c3, 0x14c, 0x216, 0x299, 0x308, 0x387, 0x02a, 0x0a5, 0x134, 0x1bb,
        0x30f, 0x380, 0x211, 0x29e, 0x133, 0x1bc, 0x02d, 0x0a2, 0x3f8, 0x377, 0x2e6, 0x269, 0x1c4, 0x14b, 0x0da, 0x055,
        0x13d, 0x1b2, 0x023, 0x0ac, 0x301, 0x38e, 0x21f, 0x290, 0x1ca, 0x145, 0x0d4, 0x05b, 0x3f6, 0x379, 0x2e8, 0x267,
        0x0d3, 0x05c, 0x1cd, 0x142, 0x2ef, 0x260, 0x3f1, 0x37e, 0x024, 0x0ab, 0x13a, 0x1b5, 0x218, 0x297, 0x306, 0x389,
        0x1d6, 0x159, 0x0c8, 0x047, 0x3ea, 0x365, 0x2f4, 0x27b, 0x121, 0x1ae, 0x03f, 0x0b0, 0x31d, 0x392, 0x203, 0x28c,
        0x038, 0x0b7, 0x126, 0x1a9, 0x204, 0x28b, 0x31a, 0x395, 0x0cf, 0x040, 0x1d1, 0x15e, 0x2f3, 0x27c, 0x3ed, 0x362,
        0x20a, 0x285, 0x314, 0x39b, 0x036, 0x0b9, 0x128, 0x1a7, 0x2fd, 0x272, 0x3e3, 0x36c, 0x0c1, 0x04e, 0x1df, 0x150,
        0x3e4, 0x36b, 0x2fa, 0x275, 0x1d8, 0x157, 0x0c6, 0x049, 0x313, 0x39c, 0x20d, 0x282, 0x12f, 0x1a0, 0x031, 0x0be
};

/**********************************************************
 * Function declarations
***********************************************************/

/**
 * @brief Write command to ADBMS6830
 *
 * @param command
 * @param data
 */
ADBMS6830_Error_t adbms6830_cmd_write(ADBMS6830_Command_t command, uint8_t *data);

/**
 * @brief Read command from ADBMS6830
 *
 * @param command
 * @param data
 * @return number of responsive chips
 */
uint32_t adbms6830_cmd_read(ADBMS6830_Command_t command, uint8_t *data_buf);


uint32_t adbms6830_cmd_poll(ADBMS6830_Command_t command);

uint32_t adbms6830_adcv();
uint32_t adbms6830_adax();
uint32_t adbms6830_adstat();

void ltc6813_adowUp();
void ltc6813_adowDown();
void adbms6830_wrcfga();
void adbms6830_wrcfgb(bool enableBalancing, const bool balanceCommands[NUM_BMS_ICS*14]);

void adbms6830_wakeup();


static uint16_t pec (const uint8_t *data , int len);
static uint16_t dpec (const uint8_t *data , int len, bool rx, uint8_t counter);

#endif