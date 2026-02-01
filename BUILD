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
    name = "release",
    srcs = [
        "//CSM/firmware:release",
        "//DUI/firmware:release",
	"//HVC/firmware:release",
        "//LVBMS/firmware:release",
        "//TSM/firmware:release",
        "//USM/firmware:release",
        "//VCU/firmware:release"
    ]
)
