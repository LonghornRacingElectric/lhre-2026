def _fmilib_repo_impl(ctx):
    """Gets FMI lib

    Args:
        ctx: Context of the build
    """
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        strip_prefix = ctx.attr.strip_prefix,
    )

    ctx.symlink(ctx.attr.fmilib_config_macos, "fmilib_config_macos.h")
    ctx.symlink(ctx.attr.fmilib_config_windows, "fmilib_config_windows.h")
    ctx.symlink(ctx.attr.fmilib_config_linux, "fmilib_config_linux.h")
    ctx.symlink(ctx.attr.expat_config, "expat_config.h")

    ctx.symlink(ctx.attr.build_file, "BUILD.bazel")

# Define the repository rule
fmilib_repository = repository_rule(
    implementation = _fmilib_repo_impl,
    attrs = {
        "url": attr.string(mandatory = True),
        "sha256": attr.string(),
        "strip_prefix": attr.string(),
        # These attributes accept Labels (references to files in your repo)
        "build_file": attr.label(mandatory = True),
        "fmilib_config_macos": attr.label(mandatory = True),
        "fmilib_config_windows": attr.label(mandatory = True),
        "fmilib_config_linux": attr.label(mandatory = True),
        "expat_config": attr.label(mandatory = True),
    },
)

# --- Bzlmod Extension Boilerplate ---
# This part allows MODULE.bazel to call your rule

def _fmilib_extension_impl(ctx):
    # This runs when Bazel resolves modules
    fmilib_repository(
        name = "fmilib",
        url = "https://github.com/modelon-community/fmi-library/archive/refs/tags/3.0.4.zip",
        strip_prefix = "fmi-library-3.0.4",
        
        # Reference your local files here
        build_file = Label("//third_party/fmilib:fmilib.BUILD"),
        fmilib_config_macos = Label("//third_party/fmilib:fmilib_config_macos.h"),
        fmilib_config_windows = Label("//third_party/fmilib:fmilib_config_windows.h"),
        fmilib_config_linux = Label("//third_party/fmilib:fmilib_config_linux.h"),
        expat_config = Label("//third_party/fmilib:expat_config.h"),
    )

fmilib_deps = module_extension(
    implementation = _fmilib_extension_impl,
)
