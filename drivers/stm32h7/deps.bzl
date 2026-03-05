"""Module extension to fetch STM32 H7 driver dependencies from GitHub."""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def _stm32h7_deps_impl(_ctx):
    git_repository(
        name = "stm32h7xx_hal_driver",
        remote = "https://github.com/STMicroelectronics/stm32h7xx-hal-driver.git",
        commit = "a1996eed9172b59887bafaaa0ea1816ea14d48b5",
        build_file = "//drivers/stm32h7:hal_driver.BUILD",
    )

    git_repository(
        name = "cmsis_device_h7",
        remote = "https://github.com/STMicroelectronics/cmsis-device-h7.git",
        commit = "8f922cdc7cc6de2344e75ddd657889f4ff761790",
        build_file = "//drivers/stm32h7:cmsis_device.BUILD",
    )

    git_repository(
        name = "cmsis_core_h7",
        remote = "https://github.com/ARM-software/CMSIS_5.git",
        commit = "2b7495b8535bdcb306dac29b9ded4cfb679d7e5c",
        build_file = "//drivers/stm32h7:cmsis_core.BUILD",
    )

stm32h7_deps = module_extension(
    implementation = _stm32h7_deps_impl,
)
