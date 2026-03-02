#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>

// Allow C++ to link against C symbols
extern "C" {
#include "longhorn/can_base.h"
extern void HAL_FDCAN_RxFifo0Callback(void *hfdcan, uint32_t RxFifo0ITs);
extern void can_start_interface(can_interface_t *interface);
}

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

// -----------------------------------------------------------------------------
// 1. The Mock Class
// -----------------------------------------------------------------------------
class MockCanHal {
public:
  MOCK_METHOD(cHAL_StatusTypeDef, Init, (void *));
  MOCK_METHOD(cHAL_StatusTypeDef, Start, (void *));
  MOCK_METHOD(cHAL_StatusTypeDef, Stop, (void *));
  MOCK_METHOD(cHAL_StatusTypeDef, ActivateNotifications,
              (void *, uint32_t, uint32_t));
  MOCK_METHOD(cHAL_StatusTypeDef, AddToQueue,
              (void *, const cFDCAN_TxHeaderTypeDef *, const uint8_t *));
  MOCK_METHOD(cHAL_StatusTypeDef, GetRxMessage,
              (void *, uint32_t, cFDCAN_RxHeaderTypeDef *, uint8_t *));
  MOCK_METHOD(cHAL_StatusTypeDef, AddFilter,
              (void *, const cFDCAN_FilterTypeDef *));
  MOCK_METHOD(uint32_t, Tick, ());
  MOCK_METHOD(uint32_t, GetRxFifoFillLevel, (void *, uint32_t));

  // Helper for packing/unpacking
  MOCK_METHOD(int, Pack, (const void *, uint8_t *));
  MOCK_METHOD(int, Unpack, (uint8_t *, const void *));
};

// -----------------------------------------------------------------------------
// 2. Trampolines (Bridging C function pointers to C++ Mock)
// -----------------------------------------------------------------------------
MockCanHal *globalMock = nullptr;

cHAL_StatusTypeDef Mock_Init(void *h) { return globalMock->Init(h); }
cHAL_StatusTypeDef Mock_Start(void *h) { return globalMock->Start(h); }
cHAL_StatusTypeDef Mock_Stop(void *h) { return globalMock->Stop(h); }
cHAL_StatusTypeDef Mock_Noti(void *h, uint32_t a, uint32_t b) {
  return globalMock->ActivateNotifications(h, a, b);
}
cHAL_StatusTypeDef Mock_AddToQ(void *h, const cFDCAN_TxHeaderTypeDef *tx,
                               const uint8_t *d) {
  return globalMock->AddToQueue(h, tx, d);
}
cHAL_StatusTypeDef Mock_GetRx(void *h, uint32_t l, cFDCAN_RxHeaderTypeDef *rx,
                              uint8_t *d) {
  return globalMock->GetRxMessage(h, l, rx, d);
}
cHAL_StatusTypeDef Mock_AddFilter(void *h, const cFDCAN_FilterTypeDef *f) {
  return globalMock->AddFilter(h, f);
}
uint32_t Mock_Tick() { return globalMock->Tick(); }

uint32_t Mock_GetTxFifoFreeLevel(void *h) { return 1; }

uint32_t Mock_GetRxFifoFillLevel(void *h, uint32_t f) {
  return globalMock->GetRxFifoFillLevel(h, f);
}

// We use real malloc/free for the tests to ensure valid memory logic
void *Real_Malloc(size_t sz) { return malloc(sz); }
void Real_Free(void *ptr) { free(ptr); }

// Mock packing function
int Mock_Pack(const void *msg, uint8_t *tx_buf) {
  return globalMock->Pack(msg, tx_buf);
}

int Mock_Unpack(uint8_t *rx_buf, const void *msg) {
  return globalMock->Unpack(rx_buf, msg);
}

// -----------------------------------------------------------------------------
// 3. The Test Fixture
// -----------------------------------------------------------------------------
class CanBaseTest : public ::testing::Test {
protected:
  void SetUp() override {
    globalMock = &mockHal;

    // Setup the configuration struct with our trampoline functions
    config.init_fn = Mock_Init;
    config.start_fn = Mock_Start;
    config.stop_fn = Mock_Stop;
    config.noti_fn = Mock_Noti;
    config.add_to_queue_fn = Mock_AddToQ;
    config.get_rx_message_fn = Mock_GetRx;
    config.add_filter_fn = Mock_AddFilter;
    config.tick_fn = Mock_Tick;
    config.get_tx_fifo_free_level_fn = Mock_GetTxFifoFreeLevel;
    config.get_rx_fifo_fill_level_fn = Mock_GetRxFifoFillLevel;
    config.malloc_fn = Real_Malloc;
    config.free_fn = Real_Free;

    // Initialize the library
    can_init(&config);

    // Setup a dummy interface handle
    test_interface.handle = (void *)0x1234;
    // Reset interface state manually since we can't easily access the
    // static global array in C
    test_interface._started = false;
    test_interface._head = NULL;
    test_interface._tail = NULL;
    test_interface._filter_index = 0;
    test_interface.dropped_packets = 0;

    for (int i = 0; i < RECEIVE_TABLE_SIZE; i++) {
      test_interface.receive_table[i] = NULL;
    }
  }

  void TearDown() override { globalMock = nullptr; }

  MockCanHal mockHal;
  can_config_t config;
  can_interface_t test_interface;
};
// -----------------------------------------------------------------------------
// 4. Unit Tests
// -----------------------------------------------------------------------------

TEST_F(CanBaseTest, RegisterInterface_OnlyInitsHardware) {
  // can_register_interface should only call Init, NOT Start or Notifications
  EXPECT_CALL(mockHal, Init(test_interface.handle));
  EXPECT_CALL(mockHal, ActivateNotifications(_, _, _)).Times(0);
  EXPECT_CALL(mockHal, Start(_)).Times(0);

  can_register_interface(&test_interface);

  // Interface should NOT be marked as started yet
  EXPECT_FALSE(test_interface._started);
}

TEST_F(CanBaseTest, StartInterface_ActivatesNotificationsAndStarts) {
  // First register
  EXPECT_CALL(mockHal, Init(test_interface.handle));
  can_register_interface(&test_interface);

  // Then start — should call ActivateNotifications then Start
  {
    InSequence seq;
    EXPECT_CALL(mockHal, ActivateNotifications(test_interface.handle,
                                               NEW_MESSAGE_FIFO0, 0));
    EXPECT_CALL(mockHal, Start(test_interface.handle));
  }

  can_start_interface(&test_interface);

  EXPECT_TRUE(test_interface._started);
}

TEST_F(CanBaseTest, GetMessageHandle_AllocatesMemory) {
  int dummy_data = 0;

  // Updated to use new signature: msg, packet_id, freq, dlc, packing_fn
  can_message_t *msg = can_get_message_handle(&dummy_data, 0x123, 100,
                                              FDCAN_DLC_BYTES_8, Mock_Pack);

  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->msg, &dummy_data);
  EXPECT_EQ(msg->packet_id, 0x123);
  EXPECT_FALSE(msg->_is_scheduled);

  // Cleanup
  free(msg);
}

TEST_F(CanBaseTest, SendImmediate_PacksAndAddsToQueue) {
  // 1. Create a message using the factory
  int payload = 42;
  // freq=0 (not periodic), dlc=8
  can_message_t *msg =
      can_get_message_handle(&payload, 0x100, 0, FDCAN_DLC_BYTES_8, Mock_Pack);

  ASSERT_NE(msg, nullptr);

  // 2. Expectation: Pack is called, then AddToQueue is called
  EXPECT_CALL(mockHal, Pack(&payload, _)).Times(1);

  EXPECT_CALL(mockHal, AddToQueue(test_interface.handle, _, _))
      .WillOnce([](void *, const cFDCAN_TxHeaderTypeDef *header,
                   const uint8_t *data) {
        EXPECT_EQ(header->Identifier, 0x100);
        EXPECT_EQ(header->DataLength, FDCAN_DLC_BYTES_8);
        return cHAL_OK;
      });

  // 3. Act
  cHAL_StatusTypeDef result = can_send_immediate(&test_interface, msg);

  EXPECT_EQ(result, cHAL_OK);

  free(msg);
}

TEST_F(CanBaseTest, Service_SendsMessage_WithPhasing) {
  // 1. Setup - Create two messages with the same period
  int payload1 = 1;
  int payload2 = 2;
  // Both have 100ms period
  can_message_t *msg1 = can_get_message_handle(&payload1, 0x200, 100,
                                               FDCAN_DLC_BYTES_8, Mock_Pack);
  can_message_t *msg2 = can_get_message_handle(&payload2, 0x201, 100,
                                               FDCAN_DLC_BYTES_8, Mock_Pack);

  ASSERT_NE(msg1, nullptr);
  ASSERT_NE(msg2, nullptr);

  // 2. Registration
  // Set start time to 1000 ms
  EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1000));

  // Register msg1 (1st msg).
  // Logic: last_tx = 1000 + (0*5) - 100 = 900. Due at 1000.
  can_register_send_packet(&test_interface, msg1);

  // Register msg2 (2nd msg).
  // Logic: last_tx = 1000 + (1*5) - 100 = 905. Due at 1005.
  can_register_send_packet(&test_interface, msg2);

  // 3. Service at T = 1000
  // Msg1 (Due 1000) should send immediately.
  // Msg2 (Due 1005) should NOT send yet (Diff 95 < 100).
  EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1000));
  EXPECT_CALL(mockHal, Pack(&payload1, _)).Times(1);

  // Expect Msg1 (0x200) to send
  EXPECT_CALL(mockHal, AddToQueue(test_interface.handle, _, _))
      .WillOnce(Invoke(
          [](void *, const cFDCAN_TxHeaderTypeDef *header, const uint8_t *) {
            EXPECT_EQ(header->Identifier, 0x200);
            return cHAL_OK;
          }));

  can_service(&test_interface);

  // 4. Service at T = 1004 (Just before Msg2 phase)
  // Msg1 sent at 1000, due 1100.
  // Msg2 due 1005.
  // Neither should send.
  EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1004));
  EXPECT_CALL(mockHal, AddToQueue(_, _, _)).Times(0);

  can_service(&test_interface);

  // 5. Service at T = 1005 (Msg2 Phase Time)
  // Msg2 is now due.
  EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1005));
  EXPECT_CALL(mockHal, Pack(&payload2, _)).Times(1);

  // Expect Msg2 (0x201) to send
  EXPECT_CALL(mockHal, AddToQueue(test_interface.handle, _, _))
      .WillOnce(Invoke(
          [](void *, const cFDCAN_TxHeaderTypeDef *header, const uint8_t *) {
            EXPECT_EQ(header->Identifier, 0x201);
            return cHAL_OK;
          }));

  can_service(&test_interface);

  // 6. Service at T = 1100 (Msg1 Second Cycle)
  // Msg1 (Period 100) due at 1000 + 100 = 1100.
  // Msg2 (Period 100) due at 1005 + 100 = 1105.
  // Only Msg1 should send.
  EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1100));
  EXPECT_CALL(mockHal, Pack(&payload1, _)).Times(1);

  EXPECT_CALL(mockHal, AddToQueue(test_interface.handle, _, _))
      .WillOnce(Invoke(
          [](void *, const cFDCAN_TxHeaderTypeDef *header, const uint8_t *) {
            EXPECT_EQ(header->Identifier, 0x200);
            return cHAL_OK;
          }));

  can_service(&test_interface);

  free(msg1);
  free(msg2);
}

TEST_F(CanBaseTest, RegisterReceivePacket_ConfiguresFilter) {
  int dummy_rx = 0;
  // Updated to use factory: msg, packet_id, unpacking_fn
  can_receive_message_t *rx_msg =
      can_get_receive_message_handle(&dummy_rx, 0x500, Mock_Unpack);

  ASSERT_NE(rx_msg, nullptr);

  // Setup expectations — no Stop/Start since we removed the stop/start cycle
  EXPECT_CALL(mockHal, Tick()).WillOnce(Return(5000));
  EXPECT_CALL(mockHal, Stop(_)).Times(0);
  EXPECT_CALL(mockHal, Start(_)).Times(0);

  // Verify the correct filter settings are passed to hardware
  EXPECT_CALL(mockHal, AddFilter(test_interface.handle, _))
      .WillOnce([](void *, const cFDCAN_FilterTypeDef *filter) {
        EXPECT_EQ(filter->FilterID1, 0x500);
        EXPECT_EQ(filter->FilterID2, 0x500);
        EXPECT_EQ(filter->FilterType, FDCAN_FILTER_DUAL);
        return cHAL_OK;
      });

  can_register_receive_packet(&test_interface, rx_msg);

  // Verify it was added to the hash table
  uint32_t expected_index = 0x500 % RECEIVE_TABLE_SIZE;
  EXPECT_EQ(test_interface.receive_table[expected_index], rx_msg);

  free(rx_msg);
}

TEST_F(CanBaseTest, RxCallback_UnpacksDataCorrectly) {
  can_reset_internals();

  // 1. Setup the Receive Message using factory
  int destination_struct = 0; // The data "model" we want to unpack into

  can_receive_message_t *rx_msg =
      can_get_receive_message_handle(&destination_struct, 0x100, Mock_Unpack);

  ASSERT_NE(rx_msg, nullptr);

  // 2. Register Interface and Packet
  // We expect initialization calls here
  EXPECT_CALL(mockHal, Init(_)).WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, ActivateNotifications(_, _, _))
      .WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, Start(_)).WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, AddFilter(_, _)).WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1000));

  can_register_interface(&test_interface);
  can_register_receive_packet(&test_interface, rx_msg);
  can_start_interface(&test_interface);

  // 3. Define Expected Data
  // We simulate receiving 2 bytes: 0xCA, 0xFE
  uint8_t simulated_rx_data[] = {0xCA, 0xFE};

  // Expectation: GetRxFifoFillLevel
  EXPECT_CALL(mockHal,
              GetRxFifoFillLevel(test_interface.handle, FDCAN_RX_FIFO0))
      .WillOnce(Return(1))
      .WillRepeatedly(Return(0));

  // 4. Expectation: GetRxMessage
  // When the callback runs, it asks HAL for data.
  EXPECT_CALL(mockHal,
              GetRxMessage(test_interface.handle, FDCAN_RX_FIFO0, _, _))
      .WillOnce(Invoke([&](void *h, uint32_t loc,
                           cFDCAN_RxHeaderTypeDef *header, uint8_t *data) {
        // Fill the header info expected by the logic
        header->Identifier = 0x100;
        header->DataLength = 2;

        // Fill the data buffer
        data[0] = simulated_rx_data[0];
        data[1] = simulated_rx_data[1];

        return cHAL_OK;
      }));

  // 5. Expectation: Unpack
  // Verify that the unpack function is called
  EXPECT_CALL(mockHal, Unpack(_, &destination_struct))
      .WillOnce(Invoke([&](uint8_t *buf, const void *msg) {
        EXPECT_EQ(buf[0], 0xCA);
        EXPECT_EQ(buf[1], 0xFE);
        return 0;
      }));

  // 6. Act: Call the HAL Callback manually
  HAL_FDCAN_RxFifo0Callback(test_interface.handle, NEW_MESSAGE_FIFO0);

  // 7. Verify internal state
  EXPECT_EQ(rx_msg->_latest_rx_ms, 1000);

  free(rx_msg);
}

TEST_F(CanBaseTest, RxCallback_HandlesHashCollisionsCorrectly) {
  // 0. Reset Global State
  can_reset_internals();

  // 1. Setup Collision IDs
  // ID_2 will hash to the same bucket as ID_1
  const uint32_t ID_1 = 0x100;
  const uint32_t ID_2 = 0x100 + RECEIVE_TABLE_SIZE;

  int struct_1 = 0;
  can_receive_message_t *rx_msg_1 =
      can_get_receive_message_handle(&struct_1, ID_1, Mock_Unpack);

  int struct_2 = 0;
  can_receive_message_t *rx_msg_2 =
      can_get_receive_message_handle(&struct_2, ID_2, Mock_Unpack);

  // 2. Setup Time Simulation
  EXPECT_CALL(mockHal, Tick())
      .WillOnce(Return(100))         // Time for rx_msg_1 init
      .WillOnce(Return(100))         // Time for rx_msg_2 init
      .WillRepeatedly(Return(2000)); // Time when packet arrives

  // 3. Register Interface and Packets
  EXPECT_CALL(mockHal, Init(_)).WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, Start(_)).WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, AddFilter(_, _)).WillRepeatedly(Return(cHAL_OK));
  EXPECT_CALL(mockHal, ActivateNotifications(_, _, _))
      .WillOnce(Return(cHAL_OK));

  can_register_interface(&test_interface);
  can_register_receive_packet(&test_interface, rx_msg_1);
  can_register_receive_packet(&test_interface, rx_msg_2);
  can_start_interface(&test_interface);

  // 4. Test Scenario: Receive the Colliding ID (ID_2)
  uint8_t simulated_data[] = {0xAA, 0xBB};

  EXPECT_CALL(mockHal,
              GetRxFifoFillLevel(test_interface.handle, FDCAN_RX_FIFO0))
      .WillOnce(Return(1))
      .WillRepeatedly(Return(0));

  EXPECT_CALL(mockHal,
              GetRxMessage(test_interface.handle, FDCAN_RX_FIFO0, _, _))
      .WillOnce(Invoke([&](void *h, uint32_t loc,
                           cFDCAN_RxHeaderTypeDef *header, uint8_t *data) {
        header->Identifier = ID_2; // Incoming packet is ID_2
        header->DataLength = 2;
        data[0] = simulated_data[0];
        data[1] = simulated_data[1];
        return cHAL_OK;
      }));

  // Expect Unpack to be called ONLY on struct_2
  EXPECT_CALL(mockHal, Unpack(_, &struct_2)).WillOnce(Return(0));

  // Explicitly fail if Unpack is called on struct_1
  EXPECT_CALL(mockHal, Unpack(_, &struct_1)).Times(0);

  // 5. Act
  HAL_FDCAN_RxFifo0Callback(test_interface.handle, NEW_MESSAGE_FIFO0);

  // 6. Verify State
  // rx_msg_2 should be updated to the "Receive Time" (2000)
  EXPECT_EQ(rx_msg_2->_latest_rx_ms, 2000);

  // rx_msg_1 should still be at "Initialization Time" (100)
  EXPECT_EQ(rx_msg_1->_latest_rx_ms, 100);

  free(rx_msg_1);
  free(rx_msg_2);
}
