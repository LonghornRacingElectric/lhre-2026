load("@bazel_skylib//rules:build_test.bzl", "build_test")

build_test(
    name = "vcu_firmware_build_test",
    targets = ["//VCU/firmware:release"],
)

build_test(
    name = "rust_cand_build_test",
    targets = ["//BEVO/cand:real"],
)


build_test(
    name = "dui_firmware_build_test",
    targets = ["//DUI/firmware:release"],
)

build_test(
    name = "usm_firmware_build_test",
    targets = ["//USM/firmware:release"],
)

build_test(
    name = "csm_firmware_build_test",
    targets = ["//CSM/firmware:release"],
)

build_test(
    name = "tsm_firmware_build_test",
    targets = ["//TSM/firmware:release"],
)