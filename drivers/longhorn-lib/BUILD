load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_binary")
load("//tools:outputs/build_outputs.bzl", "MCU_FLAGS")

# Longhorn Lib is defined for both Bare Metal and FreeRTOS targets.
# If using the FreeRTOS version, it is expected that the build target that includes
# longhorn_lib will have the freertos sources. (longhorn_lib is built without linking)
# Bare metal target for Longhorn Lib -- Use when NOT running FreeRTOS
cc_library(
    name = "longhorn_lib_base",
    srcs = [
        "longhorn_lib_base_srcs",
    ],
    hdrs = [
        "longhorn_lib_base_headers",
    ],
    include_prefix = "longhorn",
    includes = ["config"],
    visibility = ["//visibility:public"],
    deps = [
        "generated_can",
        "//drivers/stm32g4:freertos_headers",
        "//drivers/stm32g4:stm32_headers",
    ],
)

cc_library(
    name = "longhorn_lib_stm32g4",
    srcs = ["longhorn_lib_srcs"],
    hdrs = ["longhorn_lib_headers"],
    copts = MCU_FLAGS,
    include_prefix = "longhorn",
    includes = ["config"],
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "generated_can_stm32g4",
        "//drivers/stm32g4:freertos_headers",
        "//drivers/stm32g4:stm32_headers",
    ],
)

cc_library(
    name = "longhorn_lib_base_stm32g4",
    srcs = ["longhorn_lib_base_srcs"],
    hdrs = ["longhorn_lib_base_headers"],
    copts = MCU_FLAGS,
    include_prefix = "longhorn",
    includes = ["config"],
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "generated_can_stm32g4",
        "//drivers/stm32g4:freertos_headers",
    ],
)

cc_library(
    name = "generated_can",
    srcs = [
        "can_ids.c",
    ],
    hdrs = [
        "can_ids.h",
    ],
    include_prefix = "longhorn/can",
)

cc_library(
    name = "generated_can_stm32g4",
    srcs = [
        "can_ids.c",
    ],
    hdrs = [
        "can_ids.h",
    ],
    copts = MCU_FLAGS,
    include_prefix = "longhorn/can",
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    visibility = ["//visibility:public"],
)

genrule(
    name = "can_json",
    srcs = [
        "config/can_bitfields.csv",
        "config/can_packets.csv",
    ],
    outs = [
        "can.json",
    ],
    cmd = """$(location generate_can_json) \\
            $(location config/can_packets.csv) $(location config/can_bitfields.csv) \\
            $(location can.json)""",
    tools = [
        "generate_can_json",
    ],
)

genrule(
    name = "can_definitions",
    srcs = [
        "can.json",
    ],
    outs = [
        "can_ids.h",
        "can_ids.c",
    ],
    cmd = """$(location generate_can_lib) \\
                $(location can.json) --c-file $(location can_ids.c) \\
                --h-file $(location can_ids.h)""",
    tools = [
        "generate_can_lib",
    ],
)

py_binary(
    name = "generate_can_json",
    srcs = ["scripts/generate_can_json.py"],
)

py_binary(
    name = "generate_can_lib",
    srcs = ["scripts/generate_can_lib.py"],
)

filegroup(
    name = "longhorn_lib_base_headers",
    srcs = glob(
        [
            "**/*.h",
        ],
        exclude = [
            "tests/*.h",
            "rtos/*.h",
        ],
    ),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "longhorn_lib_base_srcs",
    srcs = glob(
        [
            "**/*.c",
        ],
        exclude = [
            "tests/*.c",
            "rtos/*.c",
        ],
    ),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "longhorn_lib_headers",
    srcs = glob(
        [
            "**/*.h",
        ],
        exclude = [
            "tests/*.h",
        ],
    ),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "longhorn_lib_srcs",
    srcs = glob(
        [
            "**/*.c",
        ],
        exclude = [
            "tests/*.c",
        ],
    ),
    visibility = ["//visibility:public"],
)
