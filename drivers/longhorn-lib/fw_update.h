#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#include "longhorn/can/can_ids.h"
#include <stdbool.h>
#include <stdint.h>

// --- Enums and Structs ---

typedef enum {
  DEVICE_ID_ALL = 0,
  DEVICE_ID_HVC,
  DEVICE_ID_VCU,
  DEVICE_ID_USM,
  DEVICE_ID_CSM,
  DEVICE_ID_TSM,
  DEVICE_ID_DUI,
  DEVICE_ID_BEVO,
  DEVICE_ID_PDU
} can_device_t;

typedef enum {
  UPDATE_COMMAND_WRITE,
  UPDATE_COMMAND_READ,
  UPDATE_COMMAND_ERASE,
  UPDATE_COMMAND_VERIFY,
  UPDATE_COMMAND_RESET,
  UPDATE_COMMAND_ABORT
} update_command_t;

typedef enum {
  UPDATE_RESPONSE_ACK,
  UPDATE_RESPONSE_NACK,
  UPDATE_RESPONSE_CRC_ERROR,
  UPDATE_RESPONSE_BUSY,
  UPDATE_RESPONSE_INVALID_ADDRESS,
  UPDATE_RESPONSE_INVALID_BLOCK_LENGTH,
  UPDATE_RESPONSE_INVALID_CRC,
  UPDATE_RESPONSE_INVALID_COMMAND,
  UPDATE_RESPONSE_INVALID_DEVICE_ID,
  UPDATE_RESPONSE_INVALID_PACKET_ID,
  UPDATE_RESPONSE_INVALID_PAYLOAD,
  UPDATE_RESPONSE_INVALID_RESPONSE,
  UPDATE_RESPONSE_INVALID_DATA,
} update_response_t;

// --- Callbacks and Functions ---

// Function pointer to write to memory
typedef void (*write_memory_fn)(uint32_t address, uint8_t *data,
                                uint16_t length);

/**
 * @brief Initializes the firmware update subsystem.
 * @param write_cb Pointer to the memory write function.
 */
void fw_update_init(write_memory_fn write_cb);

/**
 * @brief Processes the unpacked Firmware Update Command Packet.
 * @param cmd_pkt Pointer to the unpacked command struct.
 * @return Status of the operation.
 */
update_response_t
fw_update_process_command(const msg_firmware_update_command_packet_t *cmd_pkt);

/**
 * @brief Parses the raw 0x012 Firmware Update Data Packet, buffers it, and
 * writes to memory if full.
 * @param can_data Pointer to the 8-byte CAN payload.
 * @return Status of the operation. Returns UPDATE_RESPONSE_BUSY if still
 * buffering.
 */
update_response_t fw_update_process_data(const uint8_t *can_data);

#endif // FW_UPDATE_H