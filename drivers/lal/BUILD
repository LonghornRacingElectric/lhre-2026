load("@rules_cc//cc:defs.bzl", "cc_library")
load("//tools:outputs/build_outputs.bzl", "MCU_FLAGS")

cc_library(
    name = "lal",
    hdrs = [
        "ICan.hpp",
        "IDfu.hpp",
        "ILed.hpp",
        "ISystemTimer.hpp",
        "IUsb.hpp",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "lal_mock",
    hdrs = [
        "mock/MockCan.hpp",
        "mock/MockDfu.hpp",
        "mock/MockLed.hpp",
        "mock/MockSystemTimer.hpp",
        "mock/MockUsb.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [":lal"],
)

cc_library(
    name = "lal_stm32g4",
    hdrs = [
        "stm32/Stm32Can.hpp",
        "stm32/Stm32Dfu.hpp",
        "stm32/Stm32Led.hpp",
        "stm32/Stm32SystemTimer.hpp",
        "stm32/Stm32Usb.hpp",
    ],
    copts = MCU_FLAGS,
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":lal",
        "//drivers/stm32g4:stm32_headers",
        "//drivers/longhorn-lib:longhorn_led",
        "//drivers/longhorn-lib:longhorn_lib_base_stm32g4",
    ],
)

# Compiles stm32_verify.cpp to ensure compile-correctness of header-only target classes
cc_library(
    name = "lal_stm32g4_verify",
    srcs = [
        "stm32_verify.cpp",
        "verify_config/stm32g4xx_hal_conf.h",
    ],
    copts = MCU_FLAGS,
    defines = [
        "STM32G474xx",
        "USE_HAL_DRIVER",
    ],
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    includes = ["verify_config"],
    deps = [
        ":lal_stm32g4",
        "//drivers/stm32g4:freertos_headers",
    ],
)
