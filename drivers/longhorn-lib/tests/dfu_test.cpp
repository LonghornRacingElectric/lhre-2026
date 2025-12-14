#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "longhorn/dfu_base.h"
}

class MockDfuCallbacks {
   public:
    static void StaticResetCallback() {
        if (instance) instance->Reset();
    }

    static void StaticDelayCallback(uint32_t ms) {
        if (instance) instance->Delay(ms);
    }

    static void StaticPinSetCallback(void* gpiox, uint16_t pin, uint8_t state) {
        if (instance) instance->PinSet(gpiox, pin, state);
    }

    MOCK_METHOD(void, Reset, ());
    MOCK_METHOD(void, Delay, (uint32_t ms));
    MOCK_METHOD(void, PinSet, (void* gpiox, uint16_t pin, int state));

    static MockDfuCallbacks* instance;
};

MockDfuCallbacks* MockDfuCallbacks::instance = nullptr;

class DfuBaseTest : public ::testing::Test {
   protected:
    ::testing::NiceMock<MockDfuCallbacks> mockCallbacks;

    static void* DUMMY_GPIOX;
    static const uint16_t DUMMY_PIN = 5;

    void SetUp() override {
        MockDfuCallbacks::instance = &mockCallbacks;
        dfu_config config;
        config.reset_fn = MockDfuCallbacks::StaticResetCallback;
        config.delay_fn = MockDfuCallbacks::StaticDelayCallback;
        config.pin_set_fn = MockDfuCallbacks::StaticPinSetCallback;
        config.gpiox = DUMMY_GPIOX;
        config.pin = DUMMY_PIN;
        config.semaphore_id = NULL;
        config.semaphore_release_fn = NULL;
        dfu_init(config);
    }

    void TearDown() override {
        // After each test, clear the static pointer
        MockDfuCallbacks::instance = nullptr;
    }

    // Helper function to easily add data from a string
    void addData(const std::string& data) {
        dfu_receiveData(
            reinterpret_cast<uint8_t*>(const_cast<char*>(data.c_str())),
            data.length());
    }
};

void* DfuBaseTest::DUMMY_GPIOX = (void*)0xABCDE000;

TEST_F(DfuBaseTest, InitDoesNotReset) {
    // We only care about Reset() calls.
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
}

TEST_F(DfuBaseTest, NoDataNoReset) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    check_dfu();
}

TEST_F(DfuBaseTest, JunkDataNoReset) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    addData("hello world");
    check_dfu();
    addData("another line");
    check_dfu();
}

TEST_F(DfuBaseTest, PartialCommandNoReset) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    addData("update");
    check_dfu();
}

TEST_F(DfuBaseTest, FullCommandTriggersReset) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    addData("update.");
    check_dfu();
}

TEST_F(DfuBaseTest, FullCommandInMultiplePartsTriggersReset) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(1);

    addData("upda");
    check_dfu();

    addData("te.");
    check_dfu();
}

TEST_F(DfuBaseTest, CommandWithJunkPrefix) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    addData("hello world update. now");
    check_dfu();
}

TEST_F(DfuBaseTest, CommandWithJunkSuffix) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    addData("update. please");
    check_dfu();
}

TEST_F(DfuBaseTest, PartialMatchResetByBadChar) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    addData("updateX");
    check_dfu();
}

TEST_F(DfuBaseTest, PartialMatchResetsAndStartsNewMatch) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    addData("updateu");
    check_dfu();

    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    addData("pdate.");
    check_dfu();
}

TEST_F(DfuBaseTest, PartialMatchResetAndFullMatch) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    addData("updateupdate.");
    check_dfu();
}

TEST_F(DfuBaseTest, MultipleCommandsOnlyResetOnce) {
    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    addData("update.update.");
    check_dfu();
}

TEST_F(DfuBaseTest, CommandAcrossBufferWrap) {
    std::string junk(250, 'a');
    addData(junk);

    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    check_dfu();

    addData("update.");

    EXPECT_CALL(mockCallbacks, Reset()).Times(1);
    check_dfu();
}

TEST_F(DfuBaseTest, BufferOverflowPreventsMatch) {
    std::string junk(255, 'a');
    addData(junk);
    addData("update.");
    EXPECT_CALL(mockCallbacks, Reset()).Times(0);
    check_dfu();
}
