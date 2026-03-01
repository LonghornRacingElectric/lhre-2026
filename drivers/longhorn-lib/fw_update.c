#include "fw_update.h"
#include <string.h>

#define FW_BLOCK_SIZE 256
#define BYTES_PER_PACKET 7

// internal state
static uint8_t rx_buffer[FW_BLOCK_SIZE];
static uint32_t current_address = 0;
static uint16_t total_blocks = 0;
static uint8_t expected_crc = 0;
static uint16_t received_bytes = 0;
static bool is_receiving = false;

static write_memory_fn mem_write_cb = NULL;
static abort_update_fn abort_cb = NULL;

// standard crc8 implementation (googled, not my own)
static uint8_t calculate_crc8(const uint8_t *data, uint16_t length) {
  uint8_t crc = 0x00;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void fw_update_init(write_memory_fn write_cb, abort_update_fn abort_fn) {
  mem_write_cb = write_cb;
  abort_cb = abort_fn;
  is_receiving = false;
  received_bytes = 0;
  memset(rx_buffer, 0, FW_BLOCK_SIZE);
}

update_response_t
fw_update_process_command(const msg_firmware_update_command_packet_t *cmd_pkt) {
  if (!cmd_pkt)
    return UPDATE_RESPONSE_INVALID_DATA;

  if (cmd_pkt->command == UPDATE_COMMAND_WRITE) {
    current_address = cmd_pkt->address;
    total_blocks = cmd_pkt->num_blocks;
    expected_crc = cmd_pkt->crc;

    memset(rx_buffer, 0, FW_BLOCK_SIZE);
    received_bytes = 0;
    is_receiving = true;

    // ACK command
    return UPDATE_RESPONSE_ACK;
  } else if (cmd_pkt->command == UPDATE_COMMAND_ABORT) {
    is_receiving = false;
    received_bytes = 0;
    if (abort_cb) {
      abort_cb();
    }
    return UPDATE_RESPONSE_ACK;
  }

  // TODO: make it work with other commands
  return UPDATE_RESPONSE_INVALID_COMMAND;
}

update_response_t fw_update_process_data(const uint8_t *can_data) {
  if (!can_data)
    return UPDATE_RESPONSE_INVALID_DATA;
  if (!is_receiving)
    return UPDATE_RESPONSE_NACK;

  uint8_t index = can_data[0];
  uint16_t buffer_offset = index * BYTES_PER_PACKET;

  // if we get an index that's too biig we dont want it
  if (buffer_offset >= FW_BLOCK_SIZE) {
    return UPDATE_RESPONSE_INVALID_PAYLOAD;
  }

  // 7 bytes for now
  uint8_t bytes_to_copy = BYTES_PER_PACKET;
  if (buffer_offset + bytes_to_copy > FW_BLOCK_SIZE) {
    bytes_to_copy = FW_BLOCK_SIZE - buffer_offset;
  }

  memcpy(&rx_buffer[buffer_offset], &can_data[1], bytes_to_copy);
  received_bytes += bytes_to_copy;

  if (received_bytes >= FW_BLOCK_SIZE) {
    uint8_t calculated_crc = calculate_crc8(rx_buffer, FW_BLOCK_SIZE);
    if (calculated_crc != expected_crc) {
      is_receiving = false;
      return UPDATE_RESPONSE_CRC_ERROR;
    }

    if (mem_write_cb) {
      mem_write_cb(current_address, rx_buffer, FW_BLOCK_SIZE);
    }

    is_receiving = false;
    received_bytes = 0;

    return UPDATE_RESPONSE_ACK;
  }

  return UPDATE_RESPONSE_BUSY;
}