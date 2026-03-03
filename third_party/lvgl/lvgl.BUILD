"""BUILD overlay for LVGL v8.3.11 graphics library.

LVGL requires a project-specific lv_conf.h at compile time. Since this file
lives in the consuming project (not here), LVGL sources are exported as a
filegroup and compiled in the consumer's cc_binary where lv_conf.h is on the
include path. A separate cc_library provides the LVGL header include paths
to dependents.
"""

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Header-only library that propagates LVGL include paths to dependents.
# Consumers add this to deps so that #include "lvgl.h" and internal
# LVGL includes (e.g. "src/misc/lv_math.h") resolve correctly.
cc_library(
    name = "lvgl_headers",
    hdrs = glob(["src/**/*.h"]) + ["lvgl.h"],
    includes = ["."],
)

# Source filegroup for compilation in the consumer's cc_binary.
# These files are NOT compiled here because they need lv_conf.h
# from the consuming project on the include path.
filegroup(
    name = "lvgl_srcs",
    srcs = glob(["src/**/*.c"]),
)
