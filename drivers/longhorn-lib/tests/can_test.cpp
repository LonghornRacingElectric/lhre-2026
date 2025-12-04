#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>

// Allow C++ to link against C symbols
extern "C" {
#include "longhorn/can_base.h"
extern void HAL_FDCAN_RxFifo0Callback(void* hfdcan, uint32_t RxFifo0ITs);
}

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

// -----------------------------------------------------------------------------
// 1. The Mock Class
// -----------------------------------------------------------------------------
class MockCanHal {
   public:
    MOCK_METHOD(cHAL_StatusTypeDef, Init, (void*));
    MOCK_METHOD(cHAL_StatusTypeDef, Start, (void*));
    MOCK_METHOD(cHAL_StatusTypeDef, Stop, (void*));
    MOCK_METHOD(cHAL_StatusTypeDef, ActivateNotifications,
                (void*, uint32_t, uint32_t));
    MOCK_METHOD(cHAL_StatusTypeDef, AddToQueue,
                (void*, const cFDCAN_TxHeaderTypeDef*, const uint8_t*));
    MOCK_METHOD(cHAL_StatusTypeDef, GetRxMessage,
                (void*, uint32_t, cFDCAN_RxHeaderTypeDef*, uint8_t*));
    MOCK_METHOD(cHAL_StatusTypeDef, AddFilter,
                (void*, const cFDCAN_FilterTypeDef*));
    MOCK_METHOD(uint32_t, Tick, ());

    // Helper for packing/unpacking
    MOCK_METHOD(int, Pack, (const void*, uint8_t*));
    MOCK_METHOD(int, Unpack, (uint8_t*, const void*));
};

// -----------------------------------------------------------------------------
// 2. Trampolines (Bridging C function pointers to C++ Mock)
// -----------------------------------------------------------------------------
MockCanHal* globalMock = nullptr;

cHAL_StatusTypeDef Mock_Init(void* h) { return globalMock->Init(h); }
cHAL_StatusTypeDef Mock_Start(void* h) { return globalMock->Start(h); }
cHAL_StatusTypeDef Mock_Stop(void* h) { return globalMock->Stop(h); }
cHAL_StatusTypeDef Mock_Noti(void* h, uint32_t a, uint32_t b) {
    return globalMock->ActivateNotifications(h, a, b);
}
cHAL_StatusTypeDef Mock_AddToQ(void* h, const cFDCAN_TxHeaderTypeDef* tx,
                               const uint8_t* d) {
    return globalMock->AddToQueue(h, tx, d);
}
cHAL_StatusTypeDef Mock_GetRx(void* h, uint32_t l, cFDCAN_RxHeaderTypeDef* rx,
                              uint8_t* d) {
    return globalMock->GetRxMessage(h, l, rx, d);
}
cHAL_StatusTypeDef Mock_AddFilter(void* h, const cFDCAN_FilterTypeDef* f) {
    return globalMock->AddFilter(h, f);
}
uint32_t Mock_Tick() { return globalMock->Tick(); }

// We use real malloc/free for the tests to ensure valid memory logic
void* Real_Malloc(size_t sz) { return malloc(sz); }
void Real_Free(void* ptr) { free(ptr); }

// Mock packing function
int Mock_Pack(const void* msg, uint8_t* tx_buf) {
    return globalMock->Pack(msg, tx_buf);
}

int Mock_Unpack(uint8_t* rx_buf, const void* msg) {
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
        config.malloc_fn = Real_Malloc;
        config.free_fn = Real_Free;

        // Initialize the library
        can_init(&config);

        // Setup a dummy interface handle
        test_interface.handle = (void*)0x1234;
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

TEST_F(CanBaseTest, RegisterInterface_InitializesAndStartsHardware) {
    // Expect the sequence of calls defined in can_register_interface
    {
        InSequence seq;
        EXPECT_CALL(mockHal, Init(test_interface.handle));
        EXPECT_CALL(mockHal, ActivateNotifications(test_interface.handle,
                                                   NEW_MESSAGE_FIFO0, 0));
        EXPECT_CALL(mockHal, Start(test_interface.handle));
    }

    can_register_interface(&test_interface);

    EXPECT_TRUE(test_interface._started);
}

TEST_F(CanBaseTest, GetMessageHandle_AllocatesMemory) {
    int dummy_data = 0;
    can_message_t* msg = can_get_message_handle(&dummy_data);

    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->msg, &dummy_data);
    EXPECT_FALSE(msg->_is_scheduled);

    // Cleanup (since we used real malloc)
    free(msg);
}

TEST_F(CanBaseTest, SendImmediate_PacksAndAddsToQueue) {
    // 1. Create a dummy message
    can_message_t msg;
    int payload = 42;
    msg.msg = &payload;
    msg.dlc = FDCAN_DLC_BYTES_8;
    msg.packet_id = 0x100;
    msg.id_type = FDCAN_STANDARD_ID;
    msg.packing_fn = Mock_Pack;

    // 2. Expectation: Pack is called, then AddToQueue is called
    EXPECT_CALL(mockHal, Pack(&payload, _)).Times(1);

    EXPECT_CALL(mockHal, AddToQueue(test_interface.handle, _, _))
        .WillOnce([](void*, const cFDCAN_TxHeaderTypeDef* header,
                     const uint8_t* data) {
            EXPECT_EQ(header->Identifier, 0x100);
            EXPECT_EQ(header->DataLength, FDCAN_DLC_BYTES_8);
            return cHAL_OK;
        });

    // 3. Act
    cHAL_StatusTypeDef result = can_send_immediate(&test_interface, &msg);

    EXPECT_EQ(result, cHAL_OK);
}

TEST_F(CanBaseTest, Service_SendsMessage_WhenTimeElapsed) {
    // 1. Register a message to the interface
    can_message_t msg;
    msg.period_ms = 100;
    msg.msg = (void*)1;
    msg.packing_fn = Mock_Pack;
    msg._next = NULL;

    // Use a trampoline or logic to set init time
    EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1000));
    can_register_send_packet(&test_interface, &msg);

    // Verify it was added to linked list
    EXPECT_EQ(test_interface._head, &msg);
    EXPECT_TRUE(msg._is_scheduled);

    // 2. Call service before period has elapsed
    // Last sent: 1000. Current: 1050. Period: 100. Diff: 50. Should NOT send.
    EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1050));
    EXPECT_CALL(mockHal, AddToQueue(_, _, _)).Times(0);  // Should not occur

    can_service(&test_interface);

    // 3. Call service after period has elapsed
    // Last sent: 1000. Current: 1100. Period: 100. Diff: 100. Should SEND.
    EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1100));
    EXPECT_CALL(mockHal, Pack(_, _));
    EXPECT_CALL(mockHal, AddToQueue(_, _, _)).WillOnce(Return(cHAL_OK));

    can_service(&test_interface);
}

TEST_F(CanBaseTest, RegisterReceivePacket_ConfiguresFilter) {
    can_receive_message_t rx_msg;
    rx_msg.packet_id = 0x500;
    rx_msg._next = NULL;

    // Setup expectations
    EXPECT_CALL(mockHal, Tick()).WillOnce(Return(5000));
    EXPECT_CALL(mockHal, Stop(test_interface.handle));

    // Verify the correct filter settings are passed to hardware
    EXPECT_CALL(mockHal, AddFilter(test_interface.handle, _))
        .WillOnce([](void*, const cFDCAN_FilterTypeDef* filter) {
            EXPECT_EQ(filter->FilterID1, 0x500);
            EXPECT_EQ(filter->FilterID2, 0x500);
            EXPECT_EQ(filter->FilterType, FDCAN_FILTER_DUAL);
            return cHAL_OK;
        });

    EXPECT_CALL(mockHal, Start(test_interface.handle));

    can_register_receive_packet(&test_interface, &rx_msg);

    // Verify it was added to the hash table
    // Index = 0x500 % 8 = 1280 % 8 = 0
    uint32_t expected_index = 0x500 % RECEIVE_TABLE_SIZE;
    EXPECT_EQ(test_interface.receive_table[expected_index], &rx_msg);
}

using ::testing::Invoke;

TEST_F(CanBaseTest, RxCallback_UnpacksDataCorrectly) {
    // 1. Setup the Receive Message
    can_receive_message_t rx_msg;
    int destination_struct = 0;  // The data "model" we want to unpack into

    rx_msg.packet_id = 0x100;
    rx_msg.latest_msg = &destination_struct;
    rx_msg.unpacking_fn = Mock_Unpack;  // Use our mock trampoline
    rx_msg._next = NULL;

    // 2. Register Interface and Packet
    // We expect standard initialization calls here, so we default them to OK
    EXPECT_CALL(mockHal, Init(_)).WillRepeatedly(Return(cHAL_OK));
    EXPECT_CALL(mockHal, ActivateNotifications(_, _, _))
        .WillRepeatedly(Return(cHAL_OK));
    EXPECT_CALL(mockHal, Start(_)).WillRepeatedly(Return(cHAL_OK));
    EXPECT_CALL(mockHal, Stop(_)).WillRepeatedly(Return(cHAL_OK));
    EXPECT_CALL(mockHal, AddFilter(_, _)).WillRepeatedly(Return(cHAL_OK));
    EXPECT_CALL(mockHal, Tick()).WillRepeatedly(Return(1000));

    can_register_interface(&test_interface);
    can_register_receive_packet(&test_interface, &rx_msg);

    // 3. Define Expected Data
    // We simulate receiving 2 bytes: 0xCA, 0xFE
    uint8_t simulated_rx_data[] = {0xCA, 0xFE};

    // 4. Expectation: GetRxMessage
    // When the callback runs, it asks HAL for data. We use Invoke to write
    // to the pointers provided by the C-code.
    EXPECT_CALL(mockHal,
                GetRxMessage(test_interface.handle, FDCAN_RX_FIFO0, _, _))
        .WillOnce(Invoke([&](void* h, uint32_t loc,
                             cFDCAN_RxHeaderTypeDef* header, uint8_t* data) {
            // Fill the header info expected by the logic
            header->Identifier = 0x100;
            header->DataLength = 2;  // FDCAN_DLC_BYTES_2

            // Fill the data buffer
            data[0] = simulated_rx_data[0];
            data[1] = simulated_rx_data[1];

            return cHAL_OK;
        }));

    // 5. Expectation: Unpack
    // Verify that the unpack function is called with the data we just
    // "received"
    EXPECT_CALL(mockHal, Unpack(_, &destination_struct))
        .WillOnce(Invoke([&](uint8_t* buf, const void* msg) {
            EXPECT_EQ(buf[0], 0xCA);
            EXPECT_EQ(buf[1], 0xFE);
            return 0;
        }));

    // 6. Act: Call the HAL Callback manually
    HAL_FDCAN_RxFifo0Callback(test_interface.handle, NEW_MESSAGE_FIFO0);

    // 7. Verify internal state
    // Verify the library updated the timestamp for this message
    EXPECT_EQ(rx_msg._latest_rx_ms, 1000);
}
