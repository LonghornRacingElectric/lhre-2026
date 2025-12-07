load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "longhorn_lib_led_test",
    size = "small",
    srcs = ["led_test.cpp"],
    linkstatic = True,
    deps = [
        "//drivers/longhorn-lib:longhorn_lib_base",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "longhorn_lib_usb_test",
    size = "small",
    srcs = ["usb_test.cpp"],
    linkstatic = True,
    deps = [
        "//drivers/longhorn-lib:longhorn_lib_base",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "longhorn_lib_dfu_test",
    size = "small",
    srcs = ["dfu_test.cpp"],
    linkstatic = True,
    deps = [
        "//drivers/longhorn-lib:longhorn_lib_base",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "longhorn_lib_can_test",
    size = "medium",
    srcs = ["can_test.cpp"],
    linkstatic = True,
    deps = [
        "//drivers/longhorn-lib:longhorn_lib_base",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
