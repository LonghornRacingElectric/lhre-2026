#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#include "longhorn/can/can_ids.h"
#include "longhorn/update_protocol.h"
#include <stdbool.h>
#include <stdint.h>

// expected pointer for writing to memory
typedef void (*write_memory_fn)(uint32_t address, uint8_t *data,
                                uint16_t length);

// initializes the firmware update subsystem
void fw_update_init(write_memory_fn write_cb);

// processes the unpacked firmware update command packet
update_response_t
fw_update_process_command(const msg_firmware_update_command_packet_t *cmd_pkt);

// processes the data itself, not the command, this will call the memory writing
// function
update_response_t fw_update_process_data(const uint8_t *can_data);

#endif // FW_UPDATE_H