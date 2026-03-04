"""Module extension to fetch STM32 L4 driver dependencies from GitHub."""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def _stm32l4_deps_impl(_ctx):
    git_repository(
        name = "stm32l4xx_hal_driver",
        remote = "https://github.com/STMicroelectronics/stm32l4xx-hal-driver.git",
        commit = "f8e66b7f8db10809f91a4360c154b6304fab06ba",
        build_file = "//drivers/stm32l4:hal_driver.BUILD",
    )

    git_repository(
        name = "cmsis_device_l4",
        remote = "https://github.com/STMicroelectronics/cmsis-device-l4.git",
        commit = "05b091a90d8a8ebf386c68030c865f04c354a0ba",
        build_file = "//drivers/stm32l4:cmsis_device.BUILD",
    )

    git_repository(
        name = "stm32_mw_usb_device",
        remote = "https://github.com/STMicroelectronics/stm32-mw-usb-device.git",
        commit = "947131e000d1ecebc260f22010fa75486c50b423",
        build_file = "//drivers/stm32l4:usb_device.BUILD",
    )

    git_repository(
        name = "cmsis_core",
        remote = "https://github.com/ARM-software/CMSIS_5.git",
        commit = "55b19837f5703e418ca37894d5745b1dc05e4c91",
        build_file = "//drivers/stm32l4:cmsis_core.BUILD",
    )

stm32l4_deps = module_extension(
    implementation = _stm32l4_deps_impl,
)
