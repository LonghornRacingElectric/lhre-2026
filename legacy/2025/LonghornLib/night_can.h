/**
 ******************************************************************************
 * @file    can_driver.h
 * @brief   Header file for abstracted CAN driver for STM32H7 and STM32L4.
 * Supports multiple CAN instances.
 ******************************************************************************
 * @attention
 *
 * This code provides a basic abstraction layer for CAN communication.
 * It relies on the STM32 HAL library (H7 or L4). Ensure the appropriate
 * HAL library is included and configured in your project.
 *
 * Define either STM32H733xx or STM32L4xx (or adjust defines as needed)
 * in your project settings.
 *
 * To use multiple instances (e.g., FDCAN1 and FDCAN2):
 * 1. Declare a `NightCANDriverInstance` variable for each CAN peripheral.
 * 2. Call `CAN_Init()` for each instance, passing its address and HAL handle.
 * 3. Call other functions (`CAN_AddTxPacket`, `CAN_Service`, etc.) passing
 * the pointer to the corresponding instance structure.
 *
 ******************************************************************************
 */

#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"  // Include if NIGHTCAN_HANDLE_TYPEDEF needs it
#include "night_can_ids.h"

// --- Select the target STM32 series ---
// Define ONE of these (or similar) in your project's preprocessor settings:
// #define STM32H733xx
// #define STM32L4xx

#ifdef STM32H723xx
#define STM32H733xx
#endif

#if defined(STM32H733xx)
#include "stm32h7xx_hal.h"
#define NIGHTCAN_HANDLE_TYPEDEF FDCAN_HandleTypeDef
#define NIGHTCAN_RX_HANDLETYPEDEF FDCAN_RxHeaderTypeDef
#define NIGHTCAN_TX_HANDLETYPEDEF FDCAN_TxHeaderTypeDef
#define NIGHTCAN_FILTERTYPEDEF FDCAN_FilterTypeDef
#elif defined(STM32L496xx)
#include "stm32l4xx_hal.h"
#define NIGHTCAN_HANDLE_TYPEDEF CAN_HandleTypeDef
#define NIGHTCAN_RX_HANDLETYPEDEF CAN_RxHeaderTypeDef
#define NIGHTCAN_TX_HANDLETYPEDEF CAN_TxHeaderTypeDef
#define NIGHTCAN_FILTERTYPEDEF CAN_FilterTypeDef
#else
#error "Please define either USE_STM32H7XX_HAL or USE_STM32L4XX_HAL."
#endif

// --- Configuration ---
#define CAN_RX_BUFFER_SIZE 32    // Size of the receiving buffer per instance
#define CAN_TX_SCHEDULE_SIZE 16  // Max number of scheduled packets per instance
#define MAX_CAN_INSTANCES 2      // Maximum number of CAN instances supported

// --- Type Definitions ---

/**
 * @brief Structure for defining a CAN packet to be transmitted.
 */
typedef struct NightCANPacket {
    uint32_t id;  // CAN Identifier (Standard or Extended)
    uint8_t ide;  // Identifier Type: CAN_ID_STD or CAN_ID_EXT
    uint8_t rtr;  // Remote Transmission Request: CAN_RTR_DATA or CAN_RTR_REMOTE
    uint8_t dlc;  // Data Length Code (0-8 bytes)
    uint8_t data[8];          // Payload data
    uint32_t tx_interval_ms;  // Transmission interval in milliseconds (0 for
                              // one-shot)
    // --- Internal driver state (do not modify directly) ---
    uint32_t _last_tx_time_ms;  // Timestamp of the last transmission
    bool _is_scheduled;  // Flag indicating if the packet is in the schedule
} NightCANPacket;

/**
 * @brief Structure for storing a received CAN packet.
 */
typedef struct {
    uint32_t id;  // CAN Identifier (Standard or Extended)
    uint8_t ide;  // Identifier Type: CAN_ID_STD or CAN_ID_EXT
    uint8_t rtr;  // Remote Transmission Request: CAN_RTR_DATA or CAN_RTR_REMOTE
    uint8_t dlc;  // Data Length Code (0-8 bytes)
    uint8_t data[8];        // Payload data
    uint32_t timestamp_ms;  // Timestamp when the packet was received (based on
                            // HAL_GetTick())
    uint32_t timeout_ms;    // after how long to register timeout
    bool is_recent;         // if this packet was received after being consumed
    bool is_timed_out;  // if this packet hasnt been recieved in timeout_ms --
                        // raise fault
} NightCANReceivePacket;

/**
 * @brief CAN Driver Status Codes
 */
typedef enum {
    CAN_OK = 0,
    CAN_ERROR = 1,
    CAN_BUSY = 2,
    CAN_TIMEOUT = 3,
    CAN_BUFFER_FULL = 4,
    CAN_BUFFER_EMPTY = 5,
    CAN_INVALID_PARAM = 6,
    CAN_NOT_FOUND = 7,
    CAN_INSTANCE_NULL = 8,         // Added error code
    CAN_MAX_INSTANCES_REACHED = 9  // Added error code
} CANDriverStatus;

/**
 * @brief Structure to hold all state information for a single CAN driver
 * instance.
 */
typedef struct {
    NIGHTCAN_HANDLE_TYPEDEF
    *hcan;  // Pointer to the HAL CAN handle for this instance

    NightCANReceivePacket
        *rx_buffer[CAN_RX_BUFFER_SIZE];  // buffer of pointers to user-defined
                                         // packet "inboxes"
    uint32_t rx_buffer_count;

    // Transmit schedule buffer
    NightCANPacket *
        tx_schedule[CAN_TX_SCHEDULE_SIZE];  // Array of pointers to user packets
    uint32_t tx_schedule_count;

    // Add any other instance-specific state if needed (e.g., error flags)
    bool initialized;

    bool bus_silence; // for flashing over CAN, we set the whole bus to SILENCE.
} NightCANInstance;

// --- Function Prototypes ---

/**
 * @brief Initializes a CAN peripheral instance and the driver state for it.
 * @param instance Pointer to the NightCANDriverInstance structure for this CAN
 * peripheral.
 * @param hcan Pointer to the HAL CAN handle (e.g., &hfdcan1 for H7, &hcan1 for
 * L4). This handle must be initialized by the STM32CubeMX generated code or
 * manually before calling this function.
 * @param default_filter_id Optional default filter ID (set to 0 if not needed).
 * @param default_filter_mask Optional default filter mask (set to 0 if not
 * needed).
 * @retval CANDriverStatus status code.
 */
CANDriverStatus CAN_Init(NightCANInstance *instance,
                         NIGHTCAN_HANDLE_TYPEDEF *hcan,
                         uint32_t default_filter_id_1,
                         uint32_t default_filter_id_2,
                         uint32_t default_filter_mask_1,
                         uint32_t default_filter_mask_2);

/**
 * @brief Adds a CAN packet to the transmission schedule or sends it immediately
 * for a specific instance.
 * @param instance Pointer to the driver instance.
 * @param packet Pointer to the NightCANPacket structure containing the message
 * details.
 * @retval CANDriverStatus status code.
 */
CANDriverStatus CAN_AddTxPacket(NightCANInstance *instance,
                                NightCANPacket *packet);

/**
 * @brief Removes a previously scheduled periodic packet from the transmission
 * schedule for a specific instance.
 * @param instance Pointer to the driver instance.
 * @param packet Pointer to the NightCANPacket structure that was previously
 * added.
 * @retval CANDriverStatus status code.
 */
CANDriverStatus CAN_RemoveScheduledTxPacket(NightCANInstance *instance,
                                            NightCANPacket *packet);

/**
 * @brief Retrieves the oldest received CAN packet from the buffer for a
 * specific instance.
 * @param instance Pointer to the driver instance.
 * @param received_packet Pointer to a NightCANReceivePacket structure where the
 * data will be copied.
 * @retval CANDriverStatus status code.
 */
NightCANReceivePacket *CAN_GetReceivedPacket(NightCANInstance *instance,
                                             uint32_t id);

NightCANReceivePacket CAN_create_receive_packet(uint32_t id,
                                                uint32_t timeout_ms,
                                                uint8_t dlc);

CANDriverStatus CAN_PollReceive(NightCANInstance *instance);

NightCANInstance CAN_new_instance();

/**
 * @brief Services the CAN driver for a specific instance (handles periodic
 * transmissions). This function MUST be called periodically for each active
 * instance.
 * @param instance Pointer to the driver instance.
 */
void CAN_Service(NightCANInstance *instance);

/**
 * @brief Creates a CAN packet that you can then use in your code. Pass in the
 * parameters as described.
 * @param id
 * @param interval_ms
 * @param dlc
 * @return
 */
NightCANPacket CAN_create_packet(uint32_t id, uint32_t interval_ms,
                                 uint8_t dlc);

/**
 * @brief Configures an additional CAN filter for a specific instance.
 * @param instance Pointer to the driver instance.
 * @param filter_bank Index of the filter bank to configure.
 * @param filter_id The CAN ID to filter for.
 * @param filter_mask The mask to apply to the ID for filtering.
 * @retval CANDriverStatus status code.
 */
CANDriverStatus CAN_ConfigFilter(NightCANInstance *instance,
                                 uint32_t filter_bank, uint32_t filter_id,
                                 uint32_t filter_mask);

/* Periodic function to be called */
void CAN_periodic(NightCANInstance *instance);

void CAN_consume_packet(NightCANReceivePacket *packet);

void CAN_addReceivePacket(NightCANInstance *instance,
                          NightCANReceivePacket *packet);

void CAN_bootload_init(uint8_t BOOTLOAD_PACKET_ID);

/**
 * @brief Read an integral value (e.g., int16_t, uint32_t) from a CAN packet's
 * data buffer. Performs direct memory access via pointer casting.
 * @param T The integer type (e.g., int8_t, uint16_t, int32_t) to read as.
 * @param packet_ptr Pointer to the CAN packet structure (e.g.,
 * NightCANReceivePacket* or NightCANPacket*).
 * @param start_byte The starting byte index (0-7) within the 'data' buffer.
 * @return The value read from the buffer, interpreted as type T.
 * @warning See general warnings in file header regarding safety and alignment.
 * Example: uint16_t status = can_readInt(uint16_t, &myReceivedPacket, 2);
 */
#define CAN_readInt(T, packet_ptr, start_byte) \
    (*((T *)((uint8_t *)((packet_ptr)->data) + (start_byte))))

#define CAN_readInt_with_default(T, packet_ptr, start_byte, default) \
    ((packet_ptr)->is_recent)                                        \
        ? (*((T *)((uint8_t *)((packet_ptr)->data) + (start_byte)))) \
        : default  // Cast data to uint8_t* for pointer arithmetic

/**
 * @brief Write an integral value (e.g., int16_t, uint32_t) to a CAN packet's
 * data buffer. Performs direct memory access via pointer casting. Typically
 * used with NightCANPacket*.
 * @param T The integer type (e.g., int8_t, uint16_t, int32_t) to write as.
 * @param packet_ptr Pointer to the CAN packet structure (e.g.,
 * NightCANPacket*).
 * @param start_byte The starting byte index (0-7) within the 'data' buffer.
 * @param value The integral value to write into the buffer.
 * @warning See general warnings in file header. Does NOT update
 * packet_ptr->dlc. Example: can_writeInt(int8_t, &myTxPacket, 0, -10);
 * myTxPacket.dlc = 1; // Manually set DLC
 */
#define CAN_writeInt(T, packet_ptr, start_byte, value)          \
    (*((T *)((uint8_t *)((packet_ptr)->data) + (start_byte))) = \
         (value))  // Cast data to uint8_t*

#define CAN_setBit(packet_ptr, start_byte, bitfield_index)                    \
    do {                                                                      \
        uint8_t *byte_ptr = ((uint8_t *)((packet_ptr)->data) + (start_byte)); \
        *byte_ptr |= (uint8_t)(1U << (bitfield_index));                       \
    } while (0)

#define CAN_clearBit(packet_ptr, start_byte, bitfield_index)                  \
    do {                                                                      \
        uint8_t *byte_ptr = ((uint8_t *)((packet_ptr)->data) + (start_byte)); \
        *byte_ptr &= (uint8_t)~(1U << (bitfield_index));                      \
    } while (0)

#define CAN_writeBitfield(packet_ptr, start_byte, bitfield_index, value)  \
    if (value) {                                                          \
        *((uint8_t *)((packet_ptr)->data) + (start_byte)) |= (uint8_t)(1U << (bitfield_index));                   \
    } else {                                                              \
        *((uint8_t *)((packet_ptr)->data) + (start_byte)) &= (uint8_t)~(1U << (bitfield_index));                  \
    }

/**
 * @brief Write a floating point value to a CAN packet's data buffer after
 * scaling and conversion. Scales the float by the inverse of precision,
 * converts to integer type T, and writes it using direct memory access.
 * Typically used with NightCANPacket*.
 * @param T The target integer type (e.g., int16_t, uint16_t) to store the value
 * as in the buffer.
 * @param packet_ptr Pointer to the CAN packet structure (e.g.,
 * NightCANPacket*).
 * @param start_byte The starting byte index (0-7) within the 'data' buffer.
 * @param value The floating-point value to write.
 * @param precision The scaling factor (resolution). 'value' is divided by
 * 'precision' before conversion. Example: Store 4.85V with 0.01V precision as
 * uint16_t at byte 2: can_writeFloat(uint16_t, &myTxPacket, 2, 4.85f, 0.01f);
 * // Stored integer value will be (uint16_t)(4.85 / 0.01) = 485
 * myTxPacket.dlc = 4; // Manually set DLC (assuming bytes 0,1 also used)
 * @warning See general warnings in file header. Does NOT update
 * packet_ptr->dlc. Ensure 'precision' is not zero.
 */
#define CAN_writeFloat(T, packet_ptr, start_byte, value, precision) \
    (*((T *)((uint8_t *)((packet_ptr)->data) + (start_byte))) =     \
         (T)((value) / (precision)))  // Cast data to uint8_t*

/**
 * @brief Read a floating-point value from a CAN packet's data buffer, assuming
 * it was stored scaled. Reads an integer of type T from the buffer and scales
 * it by 'precision' to reconstruct the original floating-point value.
 * @param T The underlying integer type (e.g., int16_t, uint16_t) that was
 * stored in the buffer.
 * @param packet_ptr Pointer to the CAN packet structure (e.g.,
 * NightCANReceivePacket* or NightCANPacket*).
 * @param start_byte The starting byte index (0-7) within the 'data' buffer
 * where the integer is stored.
 * @param precision The scaling factor (resolution) that was used when the float
 * was originally stored. Example: Read value stored with 0.01 precision as
 * uint16_t at byte 2: float voltage = can_readFloat(uint16_t,
 * &myReceivedPacket, 2, 0.01f);
 * // If stored value was 485, result = (float)485 * 0.01f = 4.85f.
 * @return The reconstructed floating-point value.
 * @warning See general warnings in file header regarding safety and alignment.
 */
#define CAN_readFloat(T, packet_ptr, start_byte, precision)             \
    ((float)(*((T *)((uint8_t *)((packet_ptr)->data) + (start_byte))) * \
             (precision)))  // Cast data to uint8_t*

#define CAN_readFloat_with_default(T, packet_ptr, start_byte, precision,      \
                                   default)                                   \
    ((packet_ptr)->is_recent)                                                 \
        ? ((float)(*((T *)((uint8_t *)((packet_ptr)->data) + (start_byte))) * \
                   (precision)))                                              \
        : default  // Cast data to uint8_t*

#define CAN_readBitfield(packet_ptr, start_byte, bitfield_index) \
    (bool)(( (*(((uint8_t *)((packet_ptr)->data) + (start_byte)))) >> (bitfield_index) ) & 1)

#endif  // CAN_DRIVER_H
