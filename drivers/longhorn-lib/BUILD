load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_binary")
load("//tools:outputs/build_outputs.bzl", "MCU_FLAGS")

# Minimal LED library — no STM32/HAL dependencies, usable by any ARM target (G4, L4, etc.)
cc_library(
    name = "longhorn_led",
    srcs = ["led_base.c"],
    hdrs = ["led_base.h"],
    copts = MCU_FLAGS,
    include_prefix = "longhorn",
    visibility = ["//visibility:public"],
)

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
        "//drivers/lal",
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
    cmd = """$(location generate_can_json) $(location config/can_packets.csv) $(location config/can_bitfields.csv) $(location can.json)""",
    tools = [
        "generate_can_json",
    ],
    visibility = ["//visibility:public"],
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
    cmd = """$(location generate_can_lib) $(location can.json) --c-file $(location can_ids.c) --h-file $(location can_ids.h)""",
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

py_binary(
    name = "generate_can_proto",
    srcs = [
        "scripts/generate_can_json.py",
        "scripts/generate_can_proto.py",
    ],
)

# Generates can_packets.proto as a normal Bazel build output.
#
# Output path:
# - bazel-bin/drivers/longhorn-lib/can_packets.proto
#
# This will automatically re-run if the CSV inputs change.
genrule(
    name = "can_proto",
    srcs = [
        "config/can_bitfields.csv",
        "config/can_packets.csv",
    ],
    outs = [
        "can_packets.proto",
    ],
    cmd = """$(location generate_can_proto) $(location config/can_packets.csv) $(location config/can_bitfields.csv) --out $@""",
    tools = [
        "generate_can_proto",
    ],
    visibility = ["//visibility:public"],
)

# Writes drivers/longhorn-lib/protobuf/can_packets.proto into the workspace.
# Use: bazel run //drivers/longhorn-lib:update_can_proto
py_binary(
    name = "update_can_proto",
    srcs = [
        "scripts/generate_can_json.py",
        "scripts/generate_can_proto.py",
        "scripts/update_can_proto.py",
    ],
    data = [
        "config/can_bitfields.csv",
        "config/can_packets.csv",
    ],
)

py_binary(
    name = "generate_can_dbc",
    srcs = ["scripts/generate_can_dbc.py"],
)

genrule(
    name = "can_dbc",
    srcs = [
        "config/can_bitfields.csv",
        "config/can_packets.csv",
    ],
    outs = [
        "can.dbc",
    ],
    cmd = """$(location generate_can_dbc) $(location config/can_packets.csv) $(location config/can_bitfields.csv) $@""",
    tools = [
        "generate_can_dbc",
    ],
    visibility = ["//visibility:public"],
)

py_binary(
    name = "update_can_dbc",
    srcs = [
        "scripts/generate_can_dbc.py",
        "scripts/update_can_dbc.py",
    ],
    data = [
        "config/can_bitfields.csv",
        "config/can_packets.csv",
    ],
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
            "**/*.cpp",
        ],
        exclude = [
            "tests/*.c",
            "tests/*.cpp",
        ],
    ),
    visibility = ["//visibility:public"],
)
