/* Host execution of the real CAN base + RTOS receiver using a fake RTOS queue.
 * Run with run_can_rx_freshness_host_test.py; no MCU or GoogleTest required. */
#include "longhorn/rtos/can.h"
#include "queue.h"
#include "semphr.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void HAL_FDCAN_RxFifo0Callback(void *, uint32_t);

static uint32_t now_ms, now_us;
static unsigned critical_depth, decode_calls;
static bool reject_decode, pending;
static cFDCAN_RxHeaderTypeDef incoming;
static uint8_t incoming_data[64];
static void (*receiver)(void *);
static jmp_buf drained;
static struct {
  unsigned capacity, size, read, count;
  uint8_t storage[32][128];
} queue;

void test_enter_critical(void) { ++critical_depth; }
void test_exit_critical(void) { assert(critical_depth == 1); --critical_depth; }
QueueHandle_t xQueueCreate(UBaseType_t capacity, UBaseType_t size) {
  assert(capacity <= 32 && size <= 128);
  queue.capacity = capacity;
  queue.size = size;
  return &queue;
}
BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *wake) {
  assert(q == &queue && critical_depth == 0);
  *wake = pdFALSE;
  if (queue.count == queue.capacity) return pdFALSE;
  memcpy(queue.storage[(queue.read + queue.count) % queue.capacity], item,
         queue.size);
  ++queue.count;
  return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait) {
  (void)wait;
  assert(q == &queue && critical_depth == 0);
  if (queue.count == 0) longjmp(drained, 1);
  memcpy(item, queue.storage[queue.read], queue.size);
  queue.read = (queue.read + 1) % queue.capacity;
  --queue.count;
  return pdTRUE;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) {
  (void)s; (void)t; return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_NOT_STARTED; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, unsigned depth,
                      void *arg, UBaseType_t priority, void *handle) {
  (void)depth; (void)arg; (void)priority; (void)handle;
  if (strcmp(name, "CAN_Rx") == 0) receiver = fn;
  return pdTRUE;
}
void vTaskDelay(TickType_t t) { (void)t; }

static cHAL_StatusTypeDef init(void *handle) { (void)handle; return cHAL_OK; }
static cHAL_StatusTypeDef filter(void *h, const cFDCAN_FilterTypeDef *f) {
  (void)h; (void)f; return cHAL_OK;
}
static uint32_t tick_ms(void) { return now_ms; }
static uint32_t tick_us(void) { return now_us; }
static uint32_t fill(void *h, uint32_t fifo) {
  (void)h; (void)fifo; return pending ? 1 : 0;
}
static cHAL_StatusTypeDef get(void *h, uint32_t fifo,
                            cFDCAN_RxHeaderTypeDef *header, uint8_t *data) {
  (void)h; (void)fifo;
  *header = incoming;
  memcpy(data, incoming_data, sizeof incoming_data);
  pending = false;
  return cHAL_OK;
}
static int unpack(uint8_t *data, const void *destination) {
  assert(critical_depth == 1); /* Decode and freshness publication share lock. */
  ++decode_calls;
  if (reject_decode) return -1;
  *(uint8_t *)destination = data[0];
  return 0;
}
static void receive(can_interface_t *interface, uint32_t ms, uint32_t us,
                    uint8_t dlc, uint8_t value) {
  now_ms = ms;
  now_us = us;
  incoming.Identifier = 0x321;
  incoming.DataLength = dlc;
  incoming_data[0] = value;
  pending = true;
  HAL_FDCAN_RxFifo0Callback(interface->handle, NEW_MESSAGE_FIFO0);
}
static void drain(void) {
  assert(receiver != NULL);
  if (setjmp(drained) == 0) receiver(NULL);
  assert(critical_depth == 0);
}

int main(void) {
  can_config_t config = {0};
  config.init_fn = init;
  config.add_filter_fn = filter;
  config.tick_fn = tick_ms;
  config.tick_us_fn = tick_us;
  config.get_rx_message_fn = get;
  config.get_rx_fifo_fill_level_fn = fill;
  config.malloc_fn = malloc;
  config.free_fn = free;
  can_rtos_init(&config);
  can_rtos_start_receiver_task(1);
  can_interface_t interface = {0};
  interface.handle = &interface;
  can_register_interface(&interface);
  uint8_t payload = 77;
  can_receive_message_t *msg =
      can_get_receive_message_handle(&payload, 0x321, unpack);
  assert(msg != NULL);
  msg->expected_dlc = 8;
  can_register_receive_packet(&interface, msg);
  assert(!msg->ever_received && msg->_latest_rx_ms == 0 &&
         msg->latest_rx_us == 0 && msg->latest_dlc == 0);
  assert(message_timed_out(msg, 100) && message_timed_out(NULL, 100));
  assert(message_timed_out_sticky(msg, 100) &&
         message_timed_out_sticky(NULL, 100));

  receive(&interface, 90, 90001, 7, 1);
  assert(queue.count == 0 && decode_calls == 0 && payload == 77);
  assert(interface.dropped_packets == 1 && !msg->ever_received);
  receive(&interface, 100, 100007, 8, 42);
  assert(queue.count == 1 && !msg->ever_received && payload == 77);
  now_ms = 200;
  now_us = 200000;
  drain();
  assert(msg->ever_received && payload == 42 && msg->latest_dlc == 8);
  assert(msg->_latest_rx_ms == 100 && msg->latest_rx_us == 100007);
  assert(message_timed_out(msg, 50)); /* Queue delay must not freshen old data. */

  reject_decode = true;
  receive(&interface, 300, 300002, 8, 43);
  drain();
  assert(payload == 42 && msg->_latest_rx_ms == 100 &&
         msg->latest_rx_us == 100007);
  reject_decode = false;
  for (unsigned i = 0; i < 32; ++i)
    receive(&interface, 400 + i, 400001 + i * 1000, 8, (uint8_t)i);
  receive(&interface, 999, 999999, 8, 99);
  assert(msg->rx_queue_drops == 1 && msg->_latest_rx_ms == 100);
  drain();
  assert(payload == 31 && msg->_latest_rx_ms == 431 &&
         msg->latest_rx_us == 431001);

  /* Actual CAN FD DLC 9 means 12 bytes, not nine bytes. */
  msg->expected_dlc = 12;
  receive(&interface, 1000, 1000003, 9, 50);
  drain();
  assert(msg->latest_dlc == 12 && payload == 50);

  msg->expected_dlc = 8;
  receive(&interface, UINT32_MAX - 4, UINT32_MAX - 7, 8, 51);
  drain();
  now_ms = 3;
  assert(!message_timed_out(msg, 10));
  now_ms = 5;
  assert(message_timed_out_sticky(msg, 10));
  receive(&interface, 5, 5003, 8, 52);
  drain();
  assert(message_timed_out_sticky(msg, 10));
  assert(!message_timed_out(msg, 10));

  config.tick_us_fn = NULL;
  can_init(&config);
  receive(&interface, 600, 123456789, 8, 53);
  drain();
  assert(msg->latest_rx_us == 600000 && payload == 53);
  free(msg);
  puts("CAN RX host tests passed: decode atomicity, freshness, queue drops, DLC, clocks.");
  return 0;
}
