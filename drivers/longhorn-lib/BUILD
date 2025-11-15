load("@rules_cc//cc:defs.bzl", "cc_library")
load("//tools:outputs/build_outputs.bzl", "MCU_FLAGS")

# Longhorn Lib is defined for both Bare Metal and FreeRTOS targets.
# If using the FreeRTOS version, it is expected that the build target that includes 
# longhorn_lib will have the freertos sources. (longhorn_lib is built without linking)
cc_library(
    name = "longhorn_lib",
    srcs = ["longhorn_lib_srcs"],
    deps = ["//drivers/stm32g4:freertos_headers"],
    hdrs = ["longhorn_lib_headers"],
    visibility = ["//visibility:public"],
    includes = ["."],
    include_prefix = "longhorn",
)

# Bare metal target for Longhorn Lib -- Use when NOT running FreeRTOS
cc_library(
    name = "longhorn_lib_base",
    srcs = ["longhorn_lib_base_srcs"],
    deps = ["//drivers/stm32g4:freertos_headers"],
    hdrs = ["longhorn_lib_base_headers"],
    visibility = ["//visibility:public"],
    includes = ["."],
    include_prefix = "longhorn",
)

cc_library(
    name = "longhorn_lib_stm32g4",
    srcs = ["longhorn_lib_srcs"],
    deps = ["//drivers/stm32g4:freertos_headers"],
    hdrs = ["longhorn_lib_headers"],
    visibility = ["//visibility:public"],
    includes = ["."],
    include_prefix = "longhorn",
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    copts = MCU_FLAGS
)

cc_library(
    name = "longhorn_lib_base_stm32g4",
    srcs = ["longhorn_lib_base_srcs"],
    deps = ["//drivers/stm32g4:freertos_headers"],
    hdrs = ["longhorn_lib_base_headers"],
    visibility = ["//visibility:public"],
    includes = ["."],
    include_prefix = "longhorn",
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    copts = MCU_FLAGS
)

filegroup(
    name = "longhorn_lib_base_headers",
    srcs =  glob([
        "**/*.h",
        ],
        exclude = [
            "tests/*.h",
            "rtos/*.h"
        ] 
    ),

    visibility = ["//visibility:public"],
)

filegroup(
    name = "longhorn_lib_base_srcs",
    srcs =  glob([
        "**/*.c",
        ],
        exclude = [
            "tests/*.c",
            "rtos/*.c",
        ]         
    ),

    visibility = ["//visibility:public"],
)

filegroup(
    name = "longhorn_lib_headers",
    srcs =  glob([
        "**/*.h",
        ],
        exclude = [
            "tests/*.h",
        ] 
    ),

    visibility = ["//visibility:public"],
)

filegroup(
    name = "longhorn_lib_srcs",
    srcs =  glob([
        "**/*.c",
        ],
        exclude = [
            "tests/*.c",
        ]         
    ),

    visibility = ["//visibility:public"],
)
