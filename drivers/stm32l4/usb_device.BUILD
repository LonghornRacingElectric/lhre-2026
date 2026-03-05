"""BUILD overlay for STMicroelectronics/stm32-mw-usb-device."""

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "usb_device_headers",
    hdrs = glob([
        "Core/Inc/**/*.h",
        "Class/CDC/Inc/**/*.h",
    ]),
    includes = [
        "Class/CDC/Inc",
        "Core/Inc",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "usb_device_srcs",
    srcs = glob(
        [
            "Core/Src/**/*.c",
            "Class/CDC/Src/**/*.c",
        ],
        exclude = ["**/*template.c"],
    ),
    visibility = ["//visibility:public"],
)
