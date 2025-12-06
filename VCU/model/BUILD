load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_binary(
    name = "vcu_model_host",
    srcs = ["main.cpp"],
    deps = ["vcu_model_2026"]
)

cc_library(
    name = "vcu_model_2026",
    srcs = glob(["src/**/*.c"]),
    hdrs = glob(["inc/**/*.h"]),
    include_prefix = "vcu_model",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "vcu_model_2026_embedded",
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    srcs = glob(["src/**/*.c"]),
    hdrs = glob(["inc/**/*.h"]),
    include_prefix = "vcu_model",
    visibility = ["//visibility:public"],
)