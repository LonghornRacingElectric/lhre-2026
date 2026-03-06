"""DFU Util Toolchain for Flashing"""
load("@rules_python//python:pip.bzl", "compile_pip_requirements")

filegroup(
    name = "dfu_flashing_script",
    srcs = ["flash.py"],
    visibility = ["//visibility:public"],
)

compile_pip_requirements(
    name = "dfu_requirements",
    src = "requirements.txt",
    requirements_txt = "requirements_lock.txt",
    tags = ["requirements"],
)

filegroup(
    name = "requirements_lock",
    srcs = ["requirements_lock.txt"],
    visibility = ["//visibility:public"],
)
