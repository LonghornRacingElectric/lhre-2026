#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "longhorn/led_base.h"
}

using ::testing::_;

class PwmControllerMock {
   public:
    MOCK_METHOD(void, pwm_start, (void* htim, uint32_t Channel));
};

class LEDTest;

static PwmControllerMock* g_mock_instance = nullptr;
extern "C" void fake_pwm_start_trampoline(void* htim, uint32_t Channel) {
    ASSERT_NE(g_mock_instance, nullptr)
        << "Mock instance was not set by test fixture SetUp()";
    g_mock_instance->pwm_start(htim, Channel);
}

class LEDTest : public ::testing::Test {
   protected:
    PwmControllerMock m_pwm_mock;
    void SetUp() override { g_mock_instance = &m_pwm_mock; }
    void TearDown() override { g_mock_instance = nullptr; }
};

TEST_F(LEDTest, LedInit_StartsAllPwmChannels) {
    volatile uint32_t ccr1, ccr2, ccr3;

    // Dummy pointer, won't actually be used
    void* fake_timer_handle = (void*)0xABC;

    rainbow_led_t led_conf = {.ccr1 = &ccr1,
                              .ccr2 = &ccr2,
                              .ccr3 = &ccr3,
                              .timer_handle = fake_timer_handle,
                              .pwm_start = fake_pwm_start_trampoline,
                              .channel1 = 1,
                              .channel2 = 2,
                              .channel3 = 3};

    EXPECT_CALL(m_pwm_mock, pwm_start(fake_timer_handle, led_conf.channel1))
        .Times(1);
    EXPECT_CALL(m_pwm_mock, pwm_start(fake_timer_handle, led_conf.channel2))
        .Times(1);
    EXPECT_CALL(m_pwm_mock, pwm_start(fake_timer_handle, led_conf.channel3))
        .Times(1);

    led_init(&led_conf);

    // Verify the CCR registers are initialized to the boot brightness
    EXPECT_EQ(ccr1, 125);
    EXPECT_EQ(ccr2, 125);
    EXPECT_EQ(ccr3, 125);
}
