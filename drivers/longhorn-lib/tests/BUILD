load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "longhorn_lib_led_test",
    size = "small",
    srcs = ["led_test.cpp"],
    deps = [
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "//drivers/longhorn-lib:longhorn_lib_base"
    ],
)

cc_test(
    name = "longhorn_lib_usb_test",
    size = "small",
    srcs = ["usb_test.cpp"],
    deps = [
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "//drivers/longhorn-lib:longhorn_lib_base"
    ],
)
