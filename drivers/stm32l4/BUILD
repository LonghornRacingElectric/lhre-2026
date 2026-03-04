"""STM32 L4 driver interface package.

Provides a unified interface to STM32 L4 drivers fetched from GitHub.
All targets here re-export from external repositories so that projects
can depend on //drivers/stm32l4:<target> without knowing the external
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
        "@cmsis_core_l4//:cmsis_core_l4_headers",
        "@cmsis_device_l4//:cmsis_device_l4_headers",
        "@stm32l4xx_hal_driver//:hal_headers",
    ],
)

# HAL driver source files for compilation
filegroup(
    name = "hal_srcs",
    srcs = ["@stm32l4xx_hal_driver//:hal_srcs"],
    visibility = ["//visibility:public"],
)

# USB Device middleware headers (Core + CDC class)
cc_library(
    name = "usb_device_headers",
    visibility = ["//visibility:public"],
    deps = [
        "@stm32_mw_usb_device//:usb_device_headers",
    ],
)

# USB Device middleware source files for compilation
filegroup(
    name = "usb_device_srcs",
    srcs = ["@stm32_mw_usb_device//:usb_device_srcs"],
    visibility = ["//visibility:public"],
)
