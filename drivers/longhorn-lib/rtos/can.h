#ifndef LONGHORN_LIB_RTOS_CAN_H
#define LONGHORN_LIB_RTOS_CAN_H

#include "FreeRTOS.h"
#include "longhorn/can_base.h"
#include "task.h"

/**
 * @brief Initialize the thread-safe CAN library wrapper.
 *        Calls can_init internally and initializes synchronization primitives.
 *
 * @param config configuration for all the functions that the lib needs to call
 */
void can_rtos_init(can_config_t* config);

/**
 * @brief Thread-safe registration of a CAN interface.
 *
 * @param interface struct of the interface needing to be registered and started
 */
void can_rtos_register_interface(can_interface_t* interface);

/**
 * @brief Thread-safe registration of a send packet.
 *
 * @param interface the interface to register the packet on
 * @param msg the message handle
 */
void can_rtos_register_send_packet(can_interface_t* interface,
                                   can_message_t* msg);

/**
 * @brief Thread-safe registration of a receive packet.
 *        The unpacking function in the msg will be wrapped to run in the
 * receiver task.
 *
 * @param interface the interface to register the packet on
 * @param msg the receive message handle
 */
void can_rtos_register_receive_packet(can_interface_t* interface,
                                      can_receive_message_t* msg);

/**
 * @brief Thread-safe immediate send.
 *
 * @param interface the interface to send on
 * @param msg the message to send
 * @return cHAL_StatusTypeDef status
 */
cHAL_StatusTypeDef can_rtos_send_immediate(can_interface_t* interface,
                                           can_message_t* msg);

/**
 * @brief Starts the high-priority transceiver task which services periodic
 * messages.
 *
 * @param priority FreeRTOS task priority
 * @return BaseType_t pdPASS if successful
 */
BaseType_t can_rtos_start_transceiver_task(UBaseType_t priority);

/**
 * @brief Starts the receiver task which processes received messages and calls
 * unpack functions.
 *
 * @param priority FreeRTOS task priority
 * @return BaseType_t pdPASS if successful
 */
BaseType_t can_rtos_start_receiver_task(UBaseType_t priority);

#endif  // LONGHORN_LIB_RTOS_CAN_H
