"""STM32 H7 driver interface package.

Provides a unified interface to STM32 H7 drivers fetched from GitHub.
All targets here re-export from external repositories so that projects
can depend on //drivers/stm32h7:<target> without knowing the external
repo layout.

Available targets:
    //drivers/stm32l4:stm32_headers       — HAL + CMSIS device + CMSIS core headers
    //drivers/stm32l4:hal_srcs            — HAL driver .c sources (exclude templates)
    //drivers/stm32l4:usb_device_headers  — USB Device middleware headers (Core + CDC)
    //drivers/stm32l4:usb_device_srcs     — USB Device middleware .c sources
"""

load("@rules_cc//cc:defs.bzl", "cc_library")

# Unified headers target: combines HAL, CMSIS device, and CMSIS core headers
# so projects only need to depend on a single target for all STM32L4 includes.
cc_library(
    name = "stm32_headers",
    visibility = ["//visibility:public"],
    deps = [
        "@cmsis_core_h7//:cmsis_core_h7_headers",
        "@cmsis_device_h7//:cmsis_device_h7_headers",
        "@stm32h7xx_hal_driver//:hal_headers",
    ],
)

# HAL driver source files for compilation
filegroup(
    name = "hal_srcs",
    srcs = ["@stm32h7xx_hal_driver//:hal_srcs"],
    visibility = ["//visibility:public"],
)
