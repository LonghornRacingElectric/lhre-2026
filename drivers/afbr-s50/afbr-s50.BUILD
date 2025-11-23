load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "afbr_s50_lib",
    
    srcs = glob(
        ["AFBR-S50/Lib/**/*.a"], 
        allow_empty = True
    ),
    
    hdrs = glob(
        ["AFBR-S50/Include/**/*.h"], 
        allow_empty = True
    ),
    
    includes = ["AFBR-S50/Include"],
    visibility = ["//visibility:public"], 
)