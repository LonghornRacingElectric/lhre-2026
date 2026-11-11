#!/usr/bin/env python3
"""Compile real CAN/RTOS RX code against minimal deterministic host RTOS stubs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "drivers/longhorn-lib"
RTOS = """
#ifndef CAN_HOST_RTOS_H
#define CAN_HOST_RTOS_H
#include <stddef.h>
#include <stdint.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
#define pdFALSE 0
#define pdTRUE 1
#define portMAX_DELAY UINT32_MAX
#define taskSCHEDULER_NOT_STARTED 0
#define pdMS_TO_TICKS(ms) (ms)
#define portYIELD_FROM_ISR(wake) ((void)(wake))
#define taskENTER_CRITICAL() test_enter_critical()
#define taskEXIT_CRITICAL() test_exit_critical()
void test_enter_critical(void);
void test_exit_critical(void);
QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t);
BaseType_t xQueueSendFromISR(QueueHandle_t, const void *, BaseType_t *);
BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
BaseType_t xTaskGetSchedulerState(void);
BaseType_t xTaskCreate(void (*)(void *), const char *, unsigned, void *, UBaseType_t, void *);
void vTaskDelay(TickType_t);
#endif
"""

with tempfile.TemporaryDirectory(prefix="orion-can-rx-") as folder:
    build = Path(folder)
    for name in ("FreeRTOS.h", "task.h", "queue.h", "semphr.h"):
        (build / name).write_text(RTOS)
    (build / "longhorn/can").mkdir(parents=True)
    (build / "longhorn/rtos").mkdir()
    for header in LIB.glob("*.h"):
        (build / "longhorn" / header.name).symlink_to(header)
    (build / "longhorn/can/can_ids.h").symlink_to(LIB / "can_ids.h")
    (build / "longhorn/rtos/can.h").symlink_to(LIB / "rtos/can.h")
    (build / "longhorn/rtos/logger.h").write_text("/* No logging in tested RX path. */\n")
    command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-sign-compare",
               "-I" + str(build), "-I" + str(ROOT)]
    command += [str(LIB / name) for name in (
        "can_base.c", "rtos/can.c", "can_ids.c", "fw_update.c", "led_base.c",
        "tests/can_rx_freshness_host_test.c")]
    command += ["-lm", "-o", str(build / "can_rx_test")]
    subprocess.run(command, check=True)
    subprocess.run([str(build / "can_rx_test")], check=True)
