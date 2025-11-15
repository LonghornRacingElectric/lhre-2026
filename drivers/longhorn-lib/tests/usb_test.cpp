#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cassert>  // For assert()
#include <cstdio>   // For snprintf
#include <string>
#include <vector>

extern "C" {
#include "usb_base.h"
void usb_println(const char* buffer);
}

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;

/**
 * @brief Mock class for the CDC (USB) peripheral.
 *
 * This class mocks the low-level transmit function that usb_base.c depends on.
 */
class CdcControllerMock {
   public:
    MOCK_METHOD(uint8_t, transmit, (uint8_t* Buf, uint16_t Len));
};

/**
 * @brief Global static pointer to the mock instance.
 *
 * This pointer is used by the C-style trampoline function to find the
 * C++ mock object created by the active test fixture.
 */
static CdcControllerMock* g_usb_mock_instance = nullptr;

/**
 * @brief C-style trampoline function (C linkage).
 *
 * This function matches the CDC_Transmit_Fn_ptr signature. It's passed to
 * usb_init() and acts as a bridge, forwarding any calls from the C library
 * to our C++ mock object.
 */
extern "C" uint8_t fake_cdc_transmit_trampoline(uint8_t* Buf, uint16_t Len) {
    // Fail fast if the test fixture didn't set the mock instance.
    // We cannot use ASSERT_NE here because this function returns uint8_t,
    // not void. ASSERT_NE would attempt a 'return;' which fails compilation.
    // A standard C assert() is the correct way to handle this
    // "impossible" condition in a helper function.
    assert(g_usb_mock_instance != nullptr &&
           "Mock instance was not set by test fixture SetUp()");

    // Forward the call to the mock method
    return g_usb_mock_instance->transmit(Buf, Len);
}

/**
 * @brief Test fixture for usb_base tests.
 *
 * This fixture manages the lifetime of the mock object and sets up the
 * global pointer and C library initialization for each test.
 */
class UsbBaseTest : public ::testing::Test {
   protected:
    // The mock object instance for this test fixture
    CdcControllerMock m_cdc_mock;

    // These variables will be used by our action to capture call arguments
    std::vector<uint8_t> m_captured_buf;
    uint16_t m_captured_len = 0;

    /**
     * @brief SetUp() is called before each test.
     */
    void SetUp() override {
        // Set the global pointer to our fixture's mock object
        g_usb_mock_instance = &m_cdc_mock;

        // Clear any captured data from a previous test run
        m_captured_buf.clear();
        m_captured_len = 0;

        // Initialize the C library under test, injecting our
        // mock trampoline function as the dependency.
        usb_init(fake_cdc_transmit_trampoline);
    }

    /**
     * @brief TearDown() is called after each test.
     */
    void TearDown() override {
        // Reset the global pointer to prevent cross-test contamination
        g_usb_mock_instance = nullptr;

        // Also reset the library's internal pointer for a clean state
        usb_init(nullptr);
    }

    /**
     * @brief Helper action to capture the buffer content and length.
     *
     * We capture the *content* because the buffer pointer (Buf)
     * might point to a temporary/stack variable within usb_printf
     * that will be invalid after the call returns.
     */
    auto CaptureBuffer() {
        return [this](uint8_t* Buf, uint16_t Len) {
            this->m_captured_buf.assign(Buf, Buf + Len);
            this->m_captured_len = Len;
            return 0;  // Return a default value (e.g., 0 for success)
        };
    }

    /**
     * @brief Helper to convert the captured buffer to a std::string
     * for easy comparison in assertions.
     */
    std::string GetCapturedString() {
        // Construct a string from the beginning of the vector to the end
        return std::string(m_captured_buf.begin(), m_captured_buf.end());
    }
};

/**
 * @brief Test that usb_println does nothing (and doesn't crash)
 * if usb_init was never called (or was reset to nullptr).
 */
TEST_F(UsbBaseTest, UsbPrintln_HandlesNullInit) {
    // Explicitly reset the init to nullptr
    usb_init(nullptr);

    EXPECT_CALL(m_cdc_mock, transmit(_, _)).Times(0);

    usb_println("This should not be sent");
}

/**
 * @brief Test sending a simple string with usb_printf.
 * Note the expectation now includes "\r\n".
 */
TEST_F(UsbBaseTest, UsbPrintf_TransmitsSimpleString) {
    const char* input_str = "Hello, World!";
    const std::string expected_str = "Hello, World!\r\n";
    const uint16_t expected_len = expected_str.length();

    EXPECT_CALL(m_cdc_mock, transmit(_, expected_len))
        .Times(1)
        .WillOnce(DoAll(CaptureBuffer(), Return(0)));

    // Call the function under test
    usb_printf(input_str);

    // Verify that the captured data matches our expectations
    EXPECT_EQ(m_captured_len, expected_len);
    EXPECT_EQ(GetCapturedString(), expected_str);
}

/**
 * @brief Test sending a string with printf-style formatting.
 * Note the expectation now includes "\r\n".
 */
TEST_F(UsbBaseTest, UsbPrintf_TransmitsFormattedString) {
    const char* format = "Value: %d, String: %s";
    int val = 123;
    const char* str = "test";

    // expected output
    char expected_output_buffer[100];
    int base_len = snprintf(expected_output_buffer,
                            sizeof(expected_output_buffer), format, val, str);
    std::string expected_str(expected_output_buffer, base_len);
    expected_str += "\r\n";
    const uint16_t expected_len = expected_str.length();

    EXPECT_CALL(m_cdc_mock, transmit(_, expected_len))
        .Times(1)
        .WillOnce(DoAll(CaptureBuffer(), Return(0)));

    usb_printf(format, val, str);

    EXPECT_EQ(m_captured_len, expected_len);
    EXPECT_EQ(GetCapturedString(), expected_str);
}

/**
 * @brief Test sending a simple, non-formatted string with usb_println.
 */
TEST_F(UsbBaseTest, UsbPrintln_TransmitsSimpleString) {
    const char* input_str = "Hello, Println!";
    const std::string expected_str = "Hello, Println!\r\n";
    const uint16_t expected_len = expected_str.length();

    EXPECT_CALL(m_cdc_mock, transmit(_, expected_len))
        .Times(1)
        .WillOnce(DoAll(CaptureBuffer(),
                        Return(0)  // Assuming 0 means success
                        ));

    // Call the function under test
    usb_println(input_str);

    // Verify that the captured data matches our expectations
    EXPECT_EQ(m_captured_len, expected_len);
    EXPECT_EQ(GetCapturedString(), expected_str);
}

/**
 * @brief Test sending an empty string with usb_println.
 */
TEST_F(UsbBaseTest, UsbPrintln_HandlesEmptyString) {
    const std::string expected_str = "\r\n";
    const uint16_t expected_len = expected_str.length();

    EXPECT_CALL(m_cdc_mock, transmit(_, expected_len))
        .Times(1)
        .WillOnce(DoAll(CaptureBuffer(), Return(0)));

    usb_println("");

    EXPECT_EQ(m_captured_len, expected_len);
    EXPECT_EQ(GetCapturedString(), expected_str);
}

/**
 * @brief Test usb_println's truncation logic.
 *
 * Assumes OUT_BUFFER_SIZE = 256.
 * We send a 300-char string. It should be truncated to 253 chars
 * (256 - 3) and then have \r\n added.
 */
TEST_F(UsbBaseTest, UsbPrintln_HandlesTruncation) {
    // Create a string of 300 'A's
    std::string long_input_str(300, 'A');

    // Create the expected truncated string (253 'A's)
    std::string expected_base_str(253, 'A');
    std::string expected_final_str = expected_base_str + "\r\n";
    const uint16_t expected_len = expected_final_str.length();

    EXPECT_CALL(m_cdc_mock, transmit(_, expected_len))
        .Times(1)
        .WillOnce(DoAll(CaptureBuffer(), Return(0)));

    // Call the function under test
    usb_println(long_input_str.c_str());

    // Verify the captured data
    EXPECT_EQ(m_captured_len, expected_len);
    EXPECT_EQ(GetCapturedString(), expected_final_str);
}