#include "dfu_base.h"

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"

#define RECEIVE_BUFFER_SIZE 256

static uint8_t receive_buffer[RECEIVE_BUFFER_SIZE];

// Circular buffer
static volatile uint32_t buffer_head = 0;
static volatile uint32_t buffer_tail = 0;
static volatile bool buffer_overflow = false;

static const uint8_t DFU_COMMAND[] = "update.";
static const uint32_t DFU_COMMAND_LEN = 7;
static uint32_t dfu_match_index = 0;

static dfu_config system_config;

void dfu_init(dfu_config config) {
    buffer_head = 0;
    buffer_tail = 0;
    buffer_overflow = false;
    dfu_match_index = 0;
    // Clear the receive buffer
    memset(receive_buffer, 0, RECEIVE_BUFFER_SIZE);
    system_config = config;
}

void dfu_receiveData(uint8_t* buf, uint32_t len) {
    // add to the circular queue
    for (uint32_t i = 0; i < len; i++) {
        uint32_t next_head = (buffer_head + 1) % RECEIVE_BUFFER_SIZE;

        if (next_head == buffer_tail) {
            // buffer got filled up
            buffer_overflow = true;
            break;
        } else {
            receive_buffer[buffer_head] = buf[i];
            buffer_head = next_head;
        }
    }

    // if we're using RTOS, we want to wake our thread so it can check the DFU
    // status
    if (system_config.semaphore_id) {
        // we have an RTOS
        bool yielding = false;
        system_config.semaphore_release_fn(system_config.semaphore_id,
                                           &yielding);

        if (yielding) {
            portYIELD_FROM_ISR(true);
        }
    }
}

void check_dfu() {
    bool command_found = false;
    uint8_t current_byte;

    uint32_t local_head;

    local_head = buffer_head;
    while (local_head != buffer_tail) {
        current_byte = receive_buffer[buffer_tail];
        if (current_byte == DFU_COMMAND[dfu_match_index]) {
            dfu_match_index++;

            if (dfu_match_index == DFU_COMMAND_LEN) {
                dfu_match_index = 0;
                command_found = true;
            }
        } else {
            dfu_match_index = 0;
            if (current_byte == DFU_COMMAND[0]) {
                dfu_match_index = 1;
            }
        }

        buffer_tail = (buffer_tail + 1) % RECEIVE_BUFFER_SIZE;
    }

    if (command_found) {
        // we got the DFU command, reset the system

        // Write the pin to be set high (this will set boot 0)
        system_config.pin_set_fn(system_config.gpiox, system_config.pin, 1);

        // delay so that the high boot0 can propagate and be detected by
        // bootloader
        system_config.delay_fn(50);

        system_config.reset_fn();
    }
}