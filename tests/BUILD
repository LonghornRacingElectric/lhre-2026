load("@bazel_skylib//rules:build_test.bzl", "build_test")

build_test(
    name = "vcu_firmware_build_test",
    targets = ["//VCU/firmware:vcu_firmware_2026"],
    target_compatible_with = select({
        "//:windows": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
)
