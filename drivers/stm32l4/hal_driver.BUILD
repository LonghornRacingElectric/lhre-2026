"""BUILD overlay for STMicroelectronics/stm32l4xx-hal-driver."""

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "hal_headers",
    hdrs = glob([
        "Inc/**/*.h",
        "Inc/Legacy/**/*.h",
    ]),
    includes = [
        "Inc",
        "Inc/Legacy",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@cmsis_core_l4//:cmsis_core_l4_headers",
        "@cmsis_device_l4//:cmsis_device_l4_headers",
    ],
)

filegroup(
    name = "hal_srcs",
    srcs = glob(
        ["Src/**/*.c"],
        exclude = ["Src/*template.c"],
    ),
    visibility = ["//visibility:public"],
)
