load("@bazel_skylib//rules:build_test.bzl", "build_test")

build_test(
    name = "vcu_firmware_build_test",
    targets = ["//VCU/firmware:release"],
)

build_test(
    name = "rust_cand_build_test",
    targets = ["//BEVO/cand:cand"],
)

build_test(
    name = "rust_dashd_build_test",
    targets = ["//BEVO/dashd:dashd"],
)
