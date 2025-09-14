platform(
    name = "arm_none_eabi",
    constraint_values = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "windows",
    constraint_values = ["@platforms//os:windows"],
)


filegroup(
    name = "Cargo.toml",
    srcs = ["Cargo.toml"],
)

filegroup(
    name = "Cargo.lock",
    srcs = ["Cargo.lock"],
)