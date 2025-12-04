#ifndef LONGHORN_LIB_CAN_BASE_H
#define LONGHORN_LIB_CAN_BASE_H

#include <stdbool.h>
#include <stdint.h>

#include "longhorn/can_hal.h"

/* Function Pointers for HAL functions */

#define RECEIVE_TABLE_SIZE 8

/* Add Message to Tx FIFO Q */
typedef cHAL_StatusTypeDef (*CAN_AddToQ_fn)(
    void* hfdcan, const cFDCAN_TxHeaderTypeDef* pTxHeader,
    const uint8_t* pTxData);

/* Init */
typedef cHAL_StatusTypeDef (*CAN_Init_fn)(void* hfdcan);

/* Start */
typedef cHAL_StatusTypeDef (*CAN_Start_fn)(void* hfdcan);

/* Stop */
typedef cHAL_StatusTypeDef (*CAN_Stop_fn)(void* hfdcan);

/* HAL Notifications Enabling */
typedef cHAL_StatusTypeDef (*CAN_ActivateNotifications_fn)(
    void* hfdcan, uint32_t ActiveITs, uint32_t BufferIndexes);

/* Packing function pointer */
typedef int (*CAN_pack_message_fn)(const void* msg, uint8_t* tx_buf);

typedef uint32_t (*Tick_fn)();

/* Define configuration for CAN */
typedef struct can_config_t {
    CAN_Init_fn init_fn;
    CAN_Start_fn start_fn;
    CAN_ActivateNotifications_fn noti_fn;
    CAN_Stop_fn stop_fn;
    CAN_AddToQ_fn add_to_queue_fn;
    Tick_fn tick_fn;
} can_config_t;

typedef struct can_handle_t {
    void* handle;
} can_handle_t;

typedef struct can_message_t {
    uint16_t period_ms;
    uint32_t packet_id;
    uint8_t id_type;
    uint8_t dlc;                    /* data length code */
    CAN_pack_message_fn packing_fn; /* Function used to pack this message */
    void* msg;                      /* Actual message */

    /* Internal State */
    uint32_t _last_tx_time_ms;
    bool _is_scheduled;
    struct can_message_t* _next;  // pointer for linked list to the next node
} can_message_t;

typedef struct can_receive_message_t {
    void* latest_msg;
    uint32_t _latest_rx_ms;
    uint32_t packet_id;
    bool timed_out;
} can_receive_message_t;

typedef struct can_interface_t {
    void* handle;
    struct can_message_t* _head;
    struct can_message_t* _tail;
    uint32_t dropped_packets;
    struct can_receive_message_t* receive_table[RECEIVE_TABLE_SIZE];
    // Internal state
    bool _started;
} can_interface_t;

/**
 * @brief Initialize the CAN library
 *
 * @param config configuration for all the functions that the lib needs to call
 */
void can_init(can_config_t* config);

/**
 * @brief For registering hfdcan1/2/3 etc.
 *
 * @param interface struct of the interface needing to be registered and started
 */
void can_register_interface(can_interface_t* interface);

can_message_t* can_get_message_handle(uint8_t msg_size);

void can_register_send_packet(can_interface_t* interface, can_message_t* msg);

cHAL_StatusTypeDef can_send_immediate(can_interface_t* interface,
                                      can_message_t* msg);

/**
 * @brief Periodically send CAN packets
 *
 * @param can
 */
void can_service(can_interface_t* can);

#endif  // LONGHORN_LIB_CAN_BASE_H