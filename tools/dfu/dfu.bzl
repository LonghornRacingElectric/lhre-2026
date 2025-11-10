"""Hermetic DFU Util Toolchain."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _dfu_util_repo_impl(ctx):
    os_name = ctx.os.name

    sha256 = "6450de30a7dcd8d8c1273f43f0b153f054fd24d85f7f38296b1ad8edbd2ddb25"
    url = "https://dfu-util.sourceforge.net/releases/dfu-util-0.11-binaries.tar.xz"
    url2 = "https://firebasestorage.googleapis.com/v0/b/bazel-binaries-lhre.firebasestorage.app/o/dfu-util-0.11-binaries.tar.xz?alt=media&token=82a16dfc-30dd-4bc2-a505-7af514d63e22"

    if os_name.startswith("linux"):
        bin_path = "linux-amd64/dfu-util"
    elif os_name.startswith("mac os x"):
        bin_path = "darwin-x86_64/dfu-util"
    elif os_name.startswith("windows"):
        bin_path = "win64/dfu-util.exe"
    else:
        fail("Unsupported OS: {}".format(os_name))

    build_file_content = """
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "dfu",
    srcs = ["{bin_path}"],
)

""".format(bin_path = bin_path)

    http_archive(
        name = "dfu",
        sha256 = sha256,
        strip_prefix = "dfu-util-0.11-binaries",
        build_file_content = build_file_content,
        urls = [url, url2],
    )

dfu = module_extension(
    implementation = _dfu_util_repo_impl,
)