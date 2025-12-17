#include "longhorn/rtos/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOSConfig.h"
#include "cmsis_os2.h"
#include "longhorn/usb_base.h"

// max size of a message that can be sent
#define MAX_LOG_MESSAGE_LEN 256

// max of 8 messages at once in the queue
#define LOG_QUEUE_LENGTH 8

#define LOGGER_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE + 256 * 8)

#define LOGGER_TASK_PRIORITY (osPriorityLow)

// Timeout -- set to 0 since we'd rather drop a message than block high priority
#define LOG_QUEUE_PUT_TIMEOUT_TICKS 0

/**
 * @brief The data structure that is sent through the queue.
 * Using a struct ensures we send a fixed-size block of memory.
 */
typedef struct {
    char buffer[MAX_LOG_MESSAGE_LEN];
} LogMessage_t;

static osMessageQueueId_t s_logQueue = NULL;
static osThreadId_t s_loggerTaskHandle = NULL;

/**
 * @brief Logging thread
 *
 * This thread waits for new messages to appear on the message queue and then
 * simply sends them over the USB serial interface. It assumes pre-formatted
 * messages. The messages can be added to the queue using the log method.
 *
 * @param argument Not used.
 */
static void loggerTask(void* argument) {
    LogMessage_t msg;
    osStatus_t status;

    for (;;) {
        // Wait forever for a message to arrive in the queue
        status = osMessageQueueGet(s_logQueue, &msg, NULL, osWaitForever);

        if (status == osOK) {
            // We successfully received a message. Send it.
            // we can safely do this without worrying about threads
            // because this is the only thread calling printf.
            usb_println(msg.buffer);
        }
    }
}

int init_logging(CDC_Transmit_Fn_ptr transmit_function) {
    // Initialize the drivers
    usb_init(transmit_function);

    // create a new queue that can be used by the logger
    s_logQueue =
        osMessageQueueNew(LOG_QUEUE_LENGTH, sizeof(LogMessage_t), NULL);
    if (s_logQueue == NULL) {
        // not enough heap memory, couldn't create a new task
        return -1;
    }

    const osThreadAttr_t loggerTask_attributes = {
        .name = "Logger",
        .stack_size = LOGGER_TASK_STACK_SIZE,
        .priority = LOGGER_TASK_PRIORITY,
    };

    s_loggerTaskHandle = osThreadNew(loggerTask, NULL, &loggerTask_attributes);

    // weren't able to create a new method
    if (s_loggerTaskHandle == NULL) {
        return -1;
    }

    return 0;
}

void log_printf(LOG_LEVEL log_level, const char* format, ...) {
    if (s_logQueue == NULL) {
        return;
    }

    LogMessage_t msg;

    // Get the current OS tick count
    uint32_t now_ticks = osKernelGetTickCount();

    char* prefix;

    switch (log_level) {
        case LOG_ERROR:
            prefix = ERROR_PREFIX;
            break;
        case LOG_WARNING:
            prefix = WARNING_PREFIX;
            break;
        case LOG_SUCCESS:
            prefix = SUCCESS_PREFIX;
            break;
        case LOG_INFO:
        default:
            prefix = INFO_PREFIX;
            break;
    }

    // Format the timestamp prefix into the buffer
    int prefix_len = snprintf(msg.buffer, MAX_LOG_MESSAGE_LEN, "[%lu] %s ",
                              now_ticks, prefix);

    // Check for encoding error or if the prefix filled the entire buffer
    if (prefix_len < 0 || prefix_len >= MAX_LOG_MESSAGE_LEN) {
        // Error or no space left for the actual message
        return;
    }

    va_list args;
    va_start(args, format);
    // Format the user's message into the buffer *after* the prefix,
    // using the remaining available space
    int msg_len = vsnprintf(msg.buffer + prefix_len,
                            MAX_LOG_MESSAGE_LEN - prefix_len, format, args);
    va_end(args);

    if (msg_len < 0) {
        return;
    }

    osMessageQueuePut(s_logQueue, &msg, 0U, LOG_QUEUE_PUT_TIMEOUT_TICKS);
}
