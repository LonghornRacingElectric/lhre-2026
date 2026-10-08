/**
 ******************************************************************************
 * @file    can_driver.c
 * @brief   Source file for abstracted CAN driver for STM32H7 and STM32L4.
 * Supports multiple CAN instances.
 ******************************************************************************
 */

#include "night_can.h"  // Include the header defining structures and prototypes

#include <stddef.h>  // For NULL
#include <stdio.h>
#include <string.h>  // For memcpy

#include "timer.h"
#include "usb_vcp.h"
#include "dfu.h"

// --- Static Variables for Instance Management ---

// Static array to hold pointers to active driver instances
static NightCANInstance *night_can_instances[MAX_CAN_INSTANCES] = {NULL};

static bool bootload_inited = false;
static uint8_t BOOTLOAD_PACKET = 0xFF;
// number of instances of the can
static uint32_t night_active_instances = 0;

#ifdef STM32L496xx
uint32_t tx_mailbox;
#endif

// --- Private Helper Functions ---

/**
 * Finds the driver instance associated with a given HAL handle.
 * @param hcan Pointer to the HAL CAN handle (use FDCAN if on H7).
 * @retval Pointer to the found NightCANDriverInstance, or NULL if not found.
 */
static NightCANInstance *find_instance(NIGHTCAN_HANDLE_TYPEDEF *hcan) {
    if (!hcan) return NULL;

    // return the instance of night can
    for (uint32_t i = 0; i < night_active_instances; ++i) {
        if (night_can_instances[i] != NULL &&
            night_can_instances[i]->hcan == hcan) {
            return night_can_instances[i];
        }
    }
    return NULL;
}

/**
 * @brief Checks if the receive buffer for a specific instance is full.
 * @param instance Pointer to the driver instance.
 * @retval true if full, false otherwise.
 */
static bool is_rx_buffer_full(NightCANInstance *instance) {
    return instance->rx_buffer_count == CAN_RX_BUFFER_SIZE;
}

/**
 * @brief Checks if the receive buffer for a specific instance is empty.
 * @param instance Pointer to the driver instance.
 * @retval true if empty, false otherwise.
 */
static bool is_rx_buffer_empty(NightCANInstance *instance) {
    return instance->rx_buffer_count == 0;
}

NightCANReceivePacket *get_packet_from_id(NightCANInstance *instance,
                                          uint32_t id) {
    for (int i = 0; i < instance->rx_buffer_count; i++) {
        if (id == instance->rx_buffer[i]->id) {
            // this is the correct packet, we can update the data on this.
            return instance->rx_buffer[i];
        }
    }

    return NULL;
}


/**
 * @brief Updates the data in the RX buffer for an ID
 * @param instance Pointer to the driver instance.
 * @param rx_header Pointer to the received message header.
 * @param rx_data Pointer to the received data payload.
 */
static void update_rx_buffer(NightCANInstance *instance,
                             NIGHTCAN_RX_HANDLETYPEDEF *rx_header,
                             uint8_t *rx_data) {

#ifdef STM32L496xx
    uint8_t temp_id = (rx_header->IDE == CAN_ID_STD) ? rx_header->StdId : rx_header->ExtId;

    if(temp_id == BOOTLOAD_PACKET) {
        boot_to_dfu();
        return;
    }


    if(temp_id == BUS_ENABLE_DISABLE_ID) {
        instance->bus_silence = (BUS_ENABLE_DISABLE_FIELD_0_TYPE) rx_data[BUS_ENABLE_DISABLE_FIELD_0_BYTE]; // updates
        return;                                                                                             // bus silence
    }

    NightCANReceivePacket *packet = get_packet_from_id(
        instance,
        (rx_header->IDE == CAN_ID_STD) ? rx_header->StdId : rx_header->ExtId);
#elif defined(STM32H733xx)
    if(rx_header->Identifier == BOOTLOAD_PACKET) {
        boot_to_dfu();
        return;
    }

    if(rx_header->Identifier == BUS_ENABLE_DISABLE_ID) {
        instance->bus_silence = (BUS_ENABLE_DISABLE_FIELD_0_TYPE) rx_data[BUS_ENABLE_DISABLE_FIELD_0_BYTE]; // updates
        return;                                                                                             // bus silence
    }

    // -- uncomment below for CAN dump --
    // char hex_str[8 * 5 + 1]; // "0xHH " ×8 + null terminator = 41 chars
    // char *p = hex_str;
    //
    // for (int i = 0; i < rx_header->DataLength; i++) {
    //     p += sprintf(p, "0x%02x ", rx_data[i]);
    // }
    // usb_printf("[%#03x] %s", rx_header->Identifier, hex_str);
    // -- end --

    NightCANReceivePacket *packet =
        get_packet_from_id(instance, rx_header->Identifier);
#endif

    // error, there is no packet given with this ID
    if (!packet) return;

    packet->timestamp_ms =
        lib_timer_elapsed_ms();  // Use HAL tick for timestamp
    packet->is_recent = true;

    uint8_t len_to_copy = (packet->dlc > 8) ? 8 : packet->dlc;
    memcpy(packet->data, rx_data, len_to_copy);
}

/**
 * @brief Sends a CAN packet immediately using HAL for a specific instance.
 * @param instance Pointer to the driver instance.
 * @param packet Pointer to the NightCANPacket to send.
 * @retval CANDriverStatus status code.
 */
static CANDriverStatus send_immediate(NightCANInstance *instance,
                                      NightCANPacket *packet) {
    if (!instance || !instance->initialized || !packet) {
        return CAN_INSTANCE_NULL;  // Or CAN_INVALID_PARAM
    }

    if (!instance->hcan) {
        return CAN_ERROR;
    }

    NIGHTCAN_TX_HANDLETYPEDEF tx_header;

#ifdef STM32L496xx
    // Prepare the HAL transmit header from our packet structure
    tx_header.DLC = packet->dlc;
    tx_header.RTR = packet->rtr;
    tx_header.IDE = packet->ide;
    if (packet->ide == CAN_ID_STD) {
        tx_header.StdId = packet->id;
    } else {
        tx_header.ExtId = packet->id;
    }
#elif defined(STM32H733xx)
    // fro fdcan in h7 chip wahooo
    tx_header.DataLength = packet->dlc;
    tx_header.IdType = packet->ide;
    tx_header.Identifier = packet->id;
    tx_header.IdType = FDCAN_STANDARD_ID;      // Standard ID type
    tx_header.TxFrameType = FDCAN_DATA_FRAME;  // Data frame
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl =
        FDCAN_NO_TX_EVENTS;  // No Tx events stored by default
#endif

    // --- Platform-Specific HAL Calls ---
#if defined(STM32H733xx)  // Use the specific define from can_driver.h
    HAL_StatusTypeDef hal_status =
        HAL_FDCAN_AddMessageToTxFifoQ(instance->hcan, &tx_header, packet->data);
#elif defined(STM32L496xx)  // Use the specific define from can_driver.h
                            // Check available mailboxes before attempting to
                            // send
    if (HAL_CAN_GetTxMailboxesFreeLevel(instance->hcan) == 0) {
        return CAN_BUSY;  // No free mailboxes
    }

    HAL_StatusTypeDef hal_status = HAL_CAN_AddTxMessage(
        instance->hcan, &tx_header, packet->data, &tx_mailbox);
#else
#error "Ay follow the notion"
    return CAN_ERROR;  // Should not reach here if header check passed
#endif

    // return the status of the HAl but with our CAN wrapper
    if (hal_status == HAL_OK) {
        return CAN_OK;
    } else if (hal_status == HAL_BUSY) {
        return CAN_BUSY;
    } else {
        return CAN_ERROR;
    }
}

// --- Public API Functions ---

/**
 * @brief Initializes a CAN peripheral instance and the driver state for it.
 */
CANDriverStatus CAN_Init(NightCANInstance *instance,
                         NIGHTCAN_HANDLE_TYPEDEF *hcan,
                         uint32_t default_filter_id_1,
                         uint32_t default_filter_id_2,
                         uint32_t default_filter_mask_1,
                         uint32_t default_filter_mask_2) {
    // Check for valid pointers and available instance slots
    if (!instance) return CAN_INVALID_PARAM;
    if (!hcan) return CAN_INVALID_PARAM;
    if (night_active_instances >= MAX_CAN_INSTANCES) {
        return CAN_MAX_INSTANCES_REACHED;
    }

    // set up the instance, memset is clearing anytthing that might be there
    // before
    memset(instance, 0, sizeof(NightCANInstance));
    instance->hcan = hcan;
    instance->initialized = false;

    // add this to our instances
    night_can_instances[night_active_instances++] = instance;

#if defined(STM32H733xx)

#elif defined(STM32L496xx)
    // to set up filter, bascially only using this on L4
    NIGHTCAN_FILTERTYPEDEF sFilterConfig;
    sFilterConfig.FilterBank = 0;  // Filter bank index (0 to 13 for single CAN)
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;   // Mask mode
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;  // 32-bit scale
    // Mask calculation for standard ID in 32-bit mask mode
    sFilterConfig.FilterIdHigh = (default_filter_id_1 << 5) & 0xFFFF;
    sFilterConfig.FilterIdLow = 0x0000;  // RTR, IDE bits = 0
    sFilterConfig.FilterMaskIdHigh = (default_filter_mask_1 << 5) & 0xFFFF;
    sFilterConfig.FilterMaskIdLow =
        0x0000 | CAN_ID_EXT |
        CAN_RTR_REMOTE;  // Mask IDE and RTR bits if needed (0 = don't care)

    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;  // Assign to FIFO0
    sFilterConfig.FilterActivation = ENABLE;            // Enable this filter
    sFilterConfig.SlaveStartFilterBank =
        14;  // Relevant for dual CAN instances, set to 14 for single CAN

    if (HAL_CAN_ConfigFilter(instance->hcan, &sFilterConfig) != HAL_OK) {
        return CAN_ERROR;  // Error configuring filter
    }
#else
#error "Ay follow the notion"
    return CAN_ERROR;
#endif

    // --- Start CAN Peripheral ---
#if defined(STM32H733xx)
    if (HAL_FDCAN_Start(instance->hcan) != HAL_OK) {
        night_active_instances--;
        night_can_instances[night_active_instances] = NULL;
        return CAN_ERROR;
    }
#elif defined(STM32L496xx)
    if (HAL_CAN_Start(instance->hcan) != HAL_OK) {
        night_active_instances--;
        night_can_instances[night_active_instances] = NULL;
        return CAN_ERROR;
    }
#endif

    instance->initialized = true;
    return CAN_OK;
}

/**
 * @brief Adds a CAN packet to the transmission schedule or sends it immediately
 * for a specific instance.
 */
CANDriverStatus CAN_AddTxPacket(NightCANInstance *instance,
                                NightCANPacket *packet) {
    // error checking
    if (!instance || !instance->initialized) return CAN_INSTANCE_NULL;
    if (!packet) return CAN_INVALID_PARAM;

    if (packet->dlc > 8) {
        return CAN_INVALID_PARAM;
    }

    // 0 interval means its nota scheduled packet
    if (packet->tx_interval_ms == 0) {
        return send_immediate(instance, packet);
    } else {
        // check if we have space to schedule this (we should be fine).
        if (instance->tx_schedule_count >= CAN_TX_SCHEDULE_SIZE) {
            return CAN_BUFFER_FULL;
        }

        // make sure this packet isnt already scheudled for this can bus -- if
        // it is, juyst uypdate the inverval
        for (uint32_t i = 0; i < instance->tx_schedule_count; i++) {
            if (instance->tx_schedule[i] == packet) {
                // Already scheduled
                return CAN_OK;
            }
        }

        // add this shire to the scheudle.
        packet->_last_tx_time_ms = lib_timer_elapsed_ms();
        packet->_is_scheduled = true;

        instance->tx_schedule[instance->tx_schedule_count++] = packet;
        return CAN_OK;
    }
}

/**
 * @brief Removes a previously scheduled periodic packet from the transmission
 * schedule for a specific instance.
 */
CANDriverStatus CAN_RemoveScheduledTxPacket(NightCANInstance *instance,
                                            NightCANPacket *packet) {
    // error check
    if (!instance || !instance->initialized) return CAN_INSTANCE_NULL;
    if (!packet) return CAN_INVALID_PARAM;

    // look for packetm if we find it, send scheudled to false and remove from
    // the transmit queue
    for (uint32_t i = 0; i < instance->tx_schedule_count; i++) {
        if (instance->tx_schedule[i] == packet) {
            instance->tx_schedule[i]->_is_scheduled = false;

            // shift everything over
            for (uint32_t j = i; j < instance->tx_schedule_count - 1; j++) {
                instance->tx_schedule[j] = instance->tx_schedule[j + 1];
            }

            instance->tx_schedule[--instance->tx_schedule_count] = NULL;
            return CAN_OK;
        }
    }

    // packet not found in instance's schedule
    return CAN_NOT_FOUND;
}

/**
 * @brief Retrieves a packet with a specific ID.
 */
NightCANReceivePacket *CAN_GetReceivedPacket(NightCANInstance *instance,
                                             uint32_t id) {
    // Check for valid instance and output pointer
    if (!instance || !instance->initialized) return NULL;

    if (is_rx_buffer_empty(instance)) {
        return NULL;
    }

    for (int i = 0; i < instance->rx_buffer_count; i++) {
        if (instance->rx_buffer[i]->id == id) {
            return instance->rx_buffer[i];
        }
    }
    // Copy data from the instance's buffer at the tail position
    return NULL;
}

NightCANInstance CAN_new_instance() {
    NightCANInstance instance = {};
    return instance;
}

/**
 * @brief Polls the hardware FIFOs for received messages and moves them to the
 * driver's buffer.
 */
CANDriverStatus CAN_PollReceive(NightCANInstance *instance) {
    if (!instance || !instance->initialized) return CAN_INSTANCE_NULL;
    if (!instance->hcan) return CAN_ERROR;

    // --- Platform specific polling ---
#if defined(STM32H733xx)
    uint32_t fill_level0 =
        HAL_FDCAN_GetRxFifoFillLevel(instance->hcan, FDCAN_RX_FIFO0);
    uint32_t fill_level1 =
        HAL_FDCAN_GetRxFifoFillLevel(instance->hcan, FDCAN_RX_FIFO1);
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    // Poll FIFO 0
    while (fill_level0 > 0) {
        if (HAL_FDCAN_GetRxMessage(instance->hcan, FDCAN_RX_FIFO0, &rx_header,
                                   rx_data) == HAL_OK) {

            update_rx_buffer(instance, &rx_header, rx_data);
        } else {
            // Error getting message from FIFO0, break out of loop so we don't
            // have error
            break;
        }
        fill_level0--;  // Decrement manually as we process one message

        // TODO: actually a good suggestion, need to test
        // Re-check fill level if GetRxMessage could fail without consuming msg?
        // fill_level0 = HAL_FDCAN_GetRxFifoFillLevel(instance->hcan,
        // FDCAN_RX_FIFO0);
    }

    // Poll FIFO 1
    while (fill_level1 > 0) {
        if (HAL_FDCAN_GetRxMessage(instance->hcan, FDCAN_RX_FIFO1, &rx_header,
                                   rx_data) == HAL_OK) {
            update_rx_buffer(instance, &rx_header, rx_data);
        } else {
            // Error getting message from FIFO1
            break;
        }
        fill_level1--;
        // fill_level1 = HAL_FDCAN_GetRxFifoFillLevel(instance->hcan,
        // FDCAN_RX_FIFO1);
    }

#elif defined(STM32L496xx)
    uint32_t fill_level0 =
        HAL_CAN_GetRxFifoFillLevel(instance->hcan, CAN_RX_FIFO0);
    uint32_t fill_level1 =
        HAL_CAN_GetRxFifoFillLevel(instance->hcan, CAN_RX_FIFO1);
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    // Poll FIFO 0
    while (fill_level0 > 0) {
        if (HAL_CAN_GetRxMessage(instance->hcan, CAN_RX_FIFO0, &rx_header,
                                 rx_data) == HAL_OK) {
            update_rx_buffer(instance, &rx_header, rx_data);
        } else {
            break;  // Error
        }
        fill_level0--;
        // fill_level0 = HAL_CAN_GetRxFifoFillLevel(instance->hcan,
        // CAN_RX_FIFO0);
    }

    // Poll FIFO 1
    while (fill_level1 > 0) {
        if (HAL_CAN_GetRxMessage(instance->hcan, CAN_RX_FIFO1, &rx_header,
                                 rx_data) == HAL_OK) {
            update_rx_buffer(instance, &rx_header, rx_data);
        } else {
            break;  // Error
        }
        fill_level1--;
        // fill_level1 = HAL_CAN_GetRxFifoFillLevel(instance->hcan,
        // CAN_RX_FIFO1);
    }
#else
#error "Ay follow the notion!"
    return CAN_ERROR;
#endif

    return CAN_OK;
}

/**
 * Mark a packet as timed out if not received in the defined ms
 * @param instance
 */
void check_timeouts(NightCANInstance *instance) {
    if(!instance || instance->bus_silence) return; // if bus silence, nothing times out

    for (int i = 0; i < instance->rx_buffer_count; i++) {
        NightCANReceivePacket *packet = instance->rx_buffer[i];
        // if the ms is defined, then we can check for timeouts, otherwise
        // assume there's no timeouts
        if (packet->timeout_ms != 0 &&
            packet->timeout_ms <
                lib_timer_elapsed_ms() - packet->timestamp_ms) {
            packet->is_timed_out = true;
        }
    }
}

void CAN_periodic(NightCANInstance *instance) {
    if (!instance || !instance->initialized) return;

    CAN_Service(instance);
    CAN_PollReceive(instance);
    check_timeouts(instance);
}

/**
 * @brief Services the CAN driver for a specific instance (handles periodic
 * transmissions).
 */
void CAN_Service(NightCANInstance *instance) {
    // make sure that the we are actually ready to send (filtering stupid
    // mistakes)
    if (!instance || !instance->initialized || !instance->hcan) return;

    if(instance->bus_silence) return; // no sending messages while silenced.

    uint32_t current_time_ms = lib_timer_elapsed_ms();

    // go through every scheduled packet and send it
    for (uint32_t i = 0; i < instance->tx_schedule_count; i++) {
        NightCANPacket *packet = instance->tx_schedule[i];

        // make sure packet exists and that it is shceudled to be sent (should
        // be), also make sure that this one is nto a one time send packet (if
        // ms interval is 0, it is meant ot only be sent once (crazy right?)) ⛺︎
        if (packet && packet->_is_scheduled && packet->tx_interval_ms > 0) {
            // make sure that we have waiited long enough
            if ((current_time_ms - packet->_last_tx_time_ms) >=
                packet->tx_interval_ms) {
                if (send_immediate(instance, packet) == CAN_OK) {
                    // Update last transmission time on successful send
                    packet->_last_tx_time_ms = current_time_ms;
                }
            }
        }
    }
}

void CAN_consume_packet(NightCANReceivePacket *packet) {
    packet->is_recent = false;
}

/**
 * Creates a packet to be used by the driver. Use this to set up the packet,
 * like a constructor! You NEED to use the packet that is returned because it is
 * the pointer that is stored in the CAN instance's struct. Lock in.
 * @param id
 * @param interval_ms
 * @param dlc
 * @return
 */
NightCANPacket CAN_create_packet(uint32_t id, uint32_t interval_ms,
                                 uint8_t dlc) {
    NightCANPacket packet = {
            .tx_interval_ms = interval_ms,
            .id = id,
            .dlc = dlc,
            .data = {0}
    };

#ifdef STM32L496xx
    packet.ide = id > 0x7FF ? CAN_ID_EXT : CAN_ID_STD;
#endif

    return packet;
};

/**
 * Creates a packet to be used by the driver. Use this to set up the packet,
 * like a constructor! You NEED to use the packet that is returned because it is
 * the pointer that is stored in the CAN instance's struct. Lock in.
 * @param id
 * @param timeout_ms
 * @param dlc
 * @return
 */
NightCANReceivePacket CAN_create_receive_packet(uint32_t id,
                                                uint32_t timeout_ms,
                                                uint8_t dlc) {
    NightCANReceivePacket packet = {.id = id, .timeout_ms = timeout_ms, .dlc = dlc,
                                    .timestamp_ms = lib_timer_elapsed_ms(), .is_recent = false};
    return packet;
};

void CAN_addReceivePacket(NightCANInstance *instance,
                          NightCANReceivePacket *packet) {
    if (get_packet_from_id(instance, packet->id) ||
        is_rx_buffer_full(instance)) {
        // packet exists or the buffers are full, don't add it
        return;
    }

    // add to the buffer and increment 🥰
    instance->rx_buffer[instance->rx_buffer_count++] = packet;
}

/**
 * @brief Configures an additional CAN filter for a specific instance.
 */
CANDriverStatus CAN_ConfigFilter(NightCANInstance *instance,
                                 uint32_t filter_bank, uint32_t filter_id,
                                 uint32_t filter_mask) {
    if (!instance || !instance->initialized || !instance->hcan)
        return CAN_INSTANCE_NULL;

#if defined(STM32H733xx)
#elif defined(STM32L496xx)
    NIGHTCAN_FILTERTYPEDEF sFilterConfig;  // Use base HAL type
    sFilterConfig.FilterBank =
        filter_bank;  // Filter bank number (0..13 or 0..27 depending on device)
    sFilterConfig.FilterMode =
        CAN_FILTERMODE_IDMASK;  // Or CAN_FILTERMODE_IDLIST
    sFilterConfig.FilterScale =
        CAN_FILTERSCALE_32BIT;  // Or CAN_FILTERSCALE_16BIT
    // Setup for 32-bit mask mode, Standard ID example:
    sFilterConfig.FilterIdHigh = (filter_id << 5) & 0xFFFF;
    sFilterConfig.FilterIdLow = 0x0000;  // Set IDE=0, RTR=0 bits if needed
    sFilterConfig.FilterMaskIdHigh = (filter_mask << 5) & 0xFFFF;
    sFilterConfig.FilterMaskIdLow =
        0x0000 | CAN_ID_EXT |
        CAN_RTR_REMOTE;  // Mask IDE/RTR bits (0 = don't care)
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;  // Or CAN_RX_FIFO1
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;  // Adjust if using dual CAN on L4

    if (HAL_CAN_ConfigFilter(instance->hcan, &sFilterConfig) != HAL_OK) {
        return CAN_ERROR;
    }
#else
#error "Ay follow the notion!"
    return CAN_ERROR;
#endif

    return CAN_OK;
}

void CAN_bootload_init(uint8_t BOOTLOAD_PACKET_ID) {
    if (bootload_inited) return;

    BOOTLOAD_PACKET = BOOTLOAD_PACKET_ID;
    bootload_inited = true;
}

