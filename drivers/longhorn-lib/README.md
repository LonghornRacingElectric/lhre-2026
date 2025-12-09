# Custom CAN Library

This library provides a flexible and abstract way to handle CAN communication on STM32G4 microcontrollers using the HAL FDCAN driver. It supports sending periodic and immediate messages, receiving messages with callbacks, and handling CAN ID filtering.

## Table of Contents

- [Introduction](#introduction)
- [Integration with Bazel](#integration-with-bazel)
- [Initialization](#initialization)
- [Defining Messages](#defining-messages)
- [Sending Messages](#sending-messages)
  - [Periodic Sending](#periodic-sending)
  - [Immediate Sending](#immediate-sending)
- [Receiving Messages](#receiving-messages)
- [Service Loop](#service-loop)
- [Example Usage](#example-usage)

## Introduction

The library is built around the concept of `can_interface_t` which represents a physical CAN interface (like FDCAN1, FDCAN2), and `can_message_t` / `can_receive_message_t` which represent the messages you want to send or receive.

It abstracts away the direct HAL calls for adding to queues or setting up filters, allowing you to focus on the data and the timing.

## Integration with Bazel

The library automatically generates C code for your CAN packets from CSV configuration files during the build process.

To use it, add the appropriate dependency to your `BUILD` file:

**For Bare Metal (No RTOS):**
```bazel
deps = [
    "//drivers/longhorn-lib:longhorn_lib_base_stm32g4",
]
```

**For FreeRTOS:**
```bazel
deps = [
    "//drivers/longhorn-lib:longhorn_lib_stm32g4",
]
```

## Initialization

Before using any CAN functions, you must initialize the library by providing a configuration struct `can_config_t`. This struct contains function pointers to the HAL functions and other utilities (like `malloc`, `free`, and a tick function).

```c
#include "longhorn/can_base.h"
#include "longhorn/can/can_ids.h" // Generated header

// ... (In your setup code)

can_config_t config = {
    .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
    .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
    .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
    .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
    .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
    .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
    .tick_fn = HAL_GetTick, // or osKernelGetTickCount for FreeRTOS
    .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
    .malloc_fn = malloc, // use pvPortMalloc in FreeRTOS
    .free_fn = free // use vPortFree in FreeRTOS
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

You do NOT need to define structs or packing functions manually. The build system generates `can_ids.h` and `can_ids.c` based on `drivers/longhorn-lib/config/can_packets.csv` and `can_bitfields.csv`.

These generated files provide:
1.  **Macros** for IDs, DLCs, and Frequencies (e.g., `INVERTER_STATUS_ID`, `INVERTER_STATUS_DLC`).
2.  **Structs** for each message (e.g., `msg_inverter_status_t`).
3.  **Packing/Unpacking Functions** (e.g., `pack_inverter_status`, `unpack_inverter_status`).

You simply instantiate the message handles using these generated artifacts.

### Creating Message Handles

Use `can_get_message_handle` for messages you intend to send, and `can_get_receive_message_handle` for messages you expect to receive.

```c
// Prepare data structure
msg_inverter_status_t tx_data = {
    .motor_speed = 1000,
    .motor_angle = 45.5f
};

// Create a handle for sending
can_message_t* tx_msg_handle = can_get_message_handle(
    &tx_data,
    INVERTER_STATUS_ID,
    INVERTER_STATUS_FREQ, // Or 0 for non-periodic
    INVERTER_STATUS_DLC,
    pack_inverter_status // Generated packing function
);

// Prepare receive structure
msg_inverter_temps_t rx_data = {0};

// Create a handle for receiving
can_receive_message_t* rx_msg_handle = can_get_receive_message_handle(
    &rx_data,
    INVERTER_TEMPS_ID,
    unpack_inverter_temps // Generated unpacking function
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

When a message is received, the library automatically handles the interrupt (`HAL_FDCAN_RxFifo0Callback`), matches the ID, calls the generated unpacking function, and updates your data structure.

**Important:** You do **NOT** need to implement `HAL_FDCAN_RxFifo0Callback` yourself.

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
#include "longhorn/can/can_ids.h" // Generated by Bazel
#include "main.h" // HAL definitions

extern FDCAN_HandleTypeDef hfdcan1;

// 1. Define Data Structures using Generated Types
msg_inverter_status_t inverter_status = {0};
msg_inverter_temps_t inverter_temps = {0};

can_interface_t can1;

void setup_can() {
    // 2. Configure Library
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

    // 3. Register Interface
    can1.handle = &hfdcan1;
    can_register_interface(&can1);

    // 4. Create & Register Send Message
    // Using generated macros and functions
    can_message_t* tx_msg = can_get_message_handle(
        &inverter_status,
        INVERTER_STATUS_ID,
        INVERTER_STATUS_FREQ,
        INVERTER_STATUS_DLC,
        pack_inverter_status
    );
    can_register_send_packet(&can1, tx_msg);

    // 5. Create & Register Receive Message
    can_receive_message_t* rx_msg = can_get_receive_message_handle(
        &inverter_temps,
        INVERTER_TEMPS_ID,
        unpack_inverter_temps
    );
    can_register_receive_packet(&can1, rx_msg);
}

void loop() {
    // Update data to be sent
    inverter_status.motor_speed++;

    // Service CAN (sends periodic messages)
    can_service(&can1);

    HAL_Delay(10);
}
```

## RTOS Integration

For FreeRTOS environments, a thread-safe wrapper is available in `longhorn/rtos/can.h`. This wrapper ensures thread safety using mutexes and offloads message unpacking from the ISR to a dedicated receiver task.

### Initialization

Use `can_rtos_init` instead of `can_init`. It initializes the underlying CAN library and creates the necessary synchronization primitives (mutexes, queues).

```c
#include "longhorn/rtos/can.h"
// ... (includes for FreeRTOS and generated IDs)

// Configuration is the same as the base version
can_config_t config = { ... };

// Initialize RTOS wrapper
can_rtos_init(&config);
```

### Tasks

You must start two tasks for the system to function:
1.  **Transceiver Task:** Handles periodic message sending.
2.  **Receiver Task:** Handles unpacking of received messages.

```c
// Start tasks with desired FreeRTOS priority (e.g., osPriorityNormal)
can_rtos_start_transceiver_task(osPriorityNormal);
can_rtos_start_receiver_task(osPriorityAboveNormal);
```

### Thread-Safe Registration & Usage

Use the `_rtos_` suffixed functions for registering interfaces and messages. These functions are thread-safe and can be called from multiple tasks.

```c
// Register Interface
can_rtos_register_interface(&can1_interface);

// Register Send Packet
can_rtos_register_send_packet(&can1_interface, tx_msg_handle);

// Register Receive Packet
// The unpacking function will be executed in the context of the Receiver Task
can_rtos_register_receive_packet(&can1_interface, rx_msg_handle);

// Immediate Send (Thread-Safe)
can_rtos_send_immediate(&can1_interface, tx_msg_handle);
```

**Note:** The `can_service` loop is handled automatically by the Transceiver Task, so you do **not** need to call it manually in your main loop.
