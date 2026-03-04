"""BUILD overlay for ARM-software/CMSIS_5 — Core headers only."""

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cmsis_core_l4_headers",
    hdrs = glob(["CMSIS/Core/Include/**/*.h"]),
    includes = ["CMSIS/Core/Include"],
    visibility = ["//visibility:public"],
)
