"""BUILD overlay for STMicroelectronics/cmsis-device-l4."""

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cmsis_device_h7_headers",
    hdrs = glob(["Include/**/*.h"]),
    includes = ["Include"],
    visibility = ["//visibility:public"],
)
