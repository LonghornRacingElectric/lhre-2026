load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "longhorn_lib",
    srcs = ["longhorn_lib_srcs"],
    deps = ["//drivers/stm32g4:freertos_headers"],
    hdrs = ["longhorn_lib_headers"],
    visibility = ["//visibility:public"],
    includes = ["."],
    include_prefix = "longhorn",
)


filegroup(
    name = "longhorn_lib_headers",
    srcs =  glob([
        "*.h",
        ],         
    ),
    visibility = ["//visibility:public"],
)


filegroup(
    name = "longhorn_lib_srcs",
    srcs =  glob([
        "*.c",
        ],         
    ),
    visibility = ["//visibility:public"],
)