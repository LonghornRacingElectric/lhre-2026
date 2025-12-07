# Custom CAN Library

This library provides a flexible and abstract way to handle CAN communication on STM32G4 microcontrollers using the HAL FDCAN driver. It supports sending periodic and immediate messages, receiving messages with callbacks, and handling CAN ID filtering.

## Table of Contents

- [Introduction](#introduction)
- [Integration](#integration)
- [Initialization](#initialization)
- [Defining Messages](#defining-messages)
  - [Packing and Unpacking](#packing-and-unpacking)
  - [Creating Message Handles](#creating-message-handles)
- [Sending Messages](#sending-messages)
  - [Periodic Sending](#periodic-sending)
  - [Immediate Sending](#immediate-sending)
- [Receiving Messages](#receiving-messages)
- [Service Loop](#service-loop)
- [Example Usage](#example-usage)

## Introduction

The library is built around the concept of `can_interface_t` which represents a physical CAN interface (like FDCAN1, FDCAN2), and `can_message_t` / `can_receive_message_t` which represent the messages you want to send or receive.

It abstracts away the direct HAL calls for adding to queues or setting up filters, allowing you to focus on the data and the timing.

## Integration

To use this library, you need to include the header:

```c
#include "longhorn/can_base.h"
```

And ensure you have the appropriate HAL headers available (e.g., `stm32g4xx_hal.h`).

## Initialization

Before using any CAN functions, you must initialize the library by providing a configuration struct `can_config_t`. This struct contains function pointers to the HAL functions and other utilities (like `malloc`, `free`, and a tick function).

```c
// Example wrapper functions to match the function signatures if needed
// Or simply cast the HAL functions if they match.

// You might need wrappers if your HAL functions take extra arguments or return different types,
// but for standard STM32 HAL, they often match or need slight adaptation.

can_config_t config = {
    .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
    .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
    .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
    .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
    .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
    .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
    .tick_fn = HAL_GetTick, // or osKernelGetTickCount for FreeRTOS
    .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
    .malloc_fn = malloc,
    .free_fn = free
};

can_init(&config);
```

### Registering an Interface

After initializing the library, you need to register the physical interfaces you want to use.

```c
can_interface_t can1_interface;

// Initialize the handle (pointer to the FDCAN handle structure from HAL)
extern FDCAN_HandleTypeDef hfdcan1;
can1_interface.handle = &hfdcan1;

// Register and start the interface
can_register_interface(&can1_interface);
```

`can_register_interface` will initialize the hardware, enable notifications, and start the FDCAN peripheral.

## Defining Messages

Messages are defined by their ID, data length (DLC), and packing/unpacking functions.

### Packing and Unpacking

You need to define functions that convert your application data structures to/from the raw byte array used by CAN.

```c
typedef struct {
    uint32_t value;
    uint8_t status;
} my_data_t;

// Packing function: serializes data into tx_buf
int pack_my_data(const void* msg, uint8_t* tx_buf) {
    const my_data_t* data = (const my_data_t*)msg;
    // Example serialization (Little Endian)
    tx_buf[0] = data->value & 0xFF;
    tx_buf[1] = (data->value >> 8) & 0xFF;
    tx_buf[2] = (data->value >> 16) & 0xFF;
    tx_buf[3] = (data->value >> 24) & 0xFF;
    tx_buf[4] = data->status;
    return 0; // Success
}

// Unpacking function: deserializes rx_buf into msg
int unpack_my_data(uint8_t* rx_buf, const void* msg) {
    my_data_t* data = (my_data_t*)msg; // Note: cast to non-const for unpacking
    data->value = rx_buf[0] | (rx_buf[1] << 8) | (rx_buf[2] << 16) | (rx_buf[3] << 24);
    data->status = rx_buf[4];
    return 0; // Success
}
```

### Creating Message Handles

Use `can_get_message_handle` for messages you intend to send, and `can_get_receive_message_handle` for messages you expect to receive.

```c
my_data_t my_tx_data = { .value = 100, .status = 1 };
my_data_t my_rx_data = { 0 };

// Create a handle for sending
// ID: 0x100, Period: 100ms, DLC: 5 bytes
can_message_t* tx_msg_handle = can_get_message_handle(
    &my_tx_data,
    0x100,
    100, // Period in ms (0 for non-periodic)
    FDCAN_DLC_BYTES_5,
    pack_my_data
);

// Create a handle for receiving
// ID: 0x200
can_receive_message_t* rx_msg_handle = can_get_receive_message_handle(
    &my_rx_data,
    0x200,
    unpack_my_data
);
```

## Sending Messages

### Periodic Sending

To send a message periodically, you must register it with the interface using `can_register_send_packet`. The library will automatically send it in the `can_service` loop based on the defined period.

```c
// Register the periodic message created above
can_register_send_packet(&can1_interface, tx_msg_handle);
```

### Immediate Sending

You can also send a message immediately, bypassing the periodic scheduler. This is useful for event-driven messages.

```c
// Send tx_msg_handle immediately
can_send_immediate(&can1_interface, tx_msg_handle);
```

## Receiving Messages

To receive messages, you must register the receive handle. This sets up the hardware filter for the specified CAN ID and registers the unpacking callback.

```c
can_register_receive_packet(&can1_interface, rx_msg_handle);
```

When a message with ID `0x200` is received, the library's internal interrupt handler (`HAL_FDCAN_RxFifo0Callback`) will:
1. Identify the interface that received the message.
2. Look up the corresponding `can_receive_message_t` handle.
3. Call the `unpacking_fn` (e.g., `unpack_my_data`) to parse the data into your structure.
4. Update the `_latest_rx_ms` timestamp.

You do NOT need to implement `HAL_FDCAN_RxFifo0Callback` yourself; the library handles this automatically.

## Service Loop

For periodic sending to work, you must call `can_service` frequently in your main loop or a periodic task.

```c
while (1) {
    can_service(&can1_interface);

    // ... other tasks
    HAL_Delay(1); // or appropriate delay
}
```

## Example Usage

### Full Example

```c
#include "longhorn/can_base.h"
#include "main.h" // HAL definitions

extern FDCAN_HandleTypeDef hfdcan1;

// 1. Define Data Structures
typedef struct {
    uint16_t rpm;
    uint16_t temp;
} motor_status_t;

motor_status_t motor_status = {0};
motor_status_t received_status = {0};

// 2. Define Pack/Unpack functions
int pack_motor_status(const void* msg, uint8_t* tx_buf) {
    const motor_status_t* s = (const motor_status_t*)msg;
    tx_buf[0] = s->rpm & 0xFF;
    tx_buf[1] = s->rpm >> 8;
    tx_buf[2] = s->temp & 0xFF;
    tx_buf[3] = s->temp >> 8;
    return 0;
}

int unpack_motor_status(uint8_t* rx_buf, const void* msg) {
    motor_status_t* s = (motor_status_t*)msg;
    s->rpm = rx_buf[0] | (rx_buf[1] << 8);
    s->temp = rx_buf[2] | (rx_buf[3] << 8);
    return 0;
}

can_interface_t can1;

void setup_can() {
    // 3. Configure Library
    can_config_t config = {
        .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
        .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
        .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
        .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
        .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
        .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
        .tick_fn = HAL_GetTick,
        .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
        .malloc_fn = malloc,
        .free_fn = free
    };
    can_init(&config);

    // 4. Register Interface
    can1.handle = &hfdcan1;
    can_register_interface(&can1);

    // 5. Create & Register Send Message (10Hz)
    can_message_t* tx_msg = can_get_message_handle(
        &motor_status, 0x123, 100, FDCAN_DLC_BYTES_4, pack_motor_status
    );
    can_register_send_packet(&can1, tx_msg);

    // 6. Create & Register Receive Message
    can_receive_message_t* rx_msg = can_get_receive_message_handle(
        &received_status, 0x124, unpack_motor_status
    );
    can_register_receive_packet(&can1, rx_msg);
}

void loop() {
    // Update data
    motor_status.rpm++;

    // Service CAN (sends periodic messages)
    can_service(&can1);

    HAL_Delay(10);
}
```
