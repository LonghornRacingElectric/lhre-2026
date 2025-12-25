load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# -------------------------------------------------------------------------
# Configuration
# -------------------------------------------------------------------------

cc_library(
    name = "config",
    hdrs = ["fmilib_config.h"],
    includes = ["."],
)

cc_library(
    name = "expat_config",
    hdrs = ["expat_config.h"],
    includes = ["."],
)

# -------------------------------------------------------------------------
# ThirdParty Dependencies
# -------------------------------------------------------------------------

# ZLIB
cc_library(
    name = "zlib",
    srcs = glob(
        [
            "ThirdParty/Zlib/zlib-1.3.1/*.c",
            "ThirdParty/Zlib/zlib-1.3.1/*.h",
        ],
        exclude = ["ThirdParty/Zlib/zlib-1.3.1/examples/**"],
    ),
    hdrs = [
        "ThirdParty/Zlib/zlib-1.3.1/zconf.h",
        "ThirdParty/Zlib/zlib-1.3.1/zlib.h",
    ],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-Wno-implicit-function-declaration",
            "-Wno-deprecated-non-prototype",
        ],
    }),
    includes = ["ThirdParty/Zlib/zlib-1.3.1"],
)

# MINIZIP
cc_library(
    name = "minizip",
    srcs = glob(
        [
            "ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/*.c",
            "ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/*.h",
        ],
        exclude = [
            "ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/iowin32.c",
            # Exclude files containing 'main' functions
            "ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/minizip.c",
            "ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/miniunz.c",
        ],
    ) + select({
        "@platforms//os:windows": ["ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/iowin32.c"],
        "//conditions:default": [],
    }),
    hdrs = glob(["ThirdParty/Zlib/zlib-1.3.1/contrib/minizip/*.h"]),
    includes = ["ThirdParty/Zlib/zlib-1.3.1/contrib/minizip"],
    deps = [":zlib"],
)

# EXPAT
cc_library(
    name = "expat",
    srcs = glob([
        "ThirdParty/Expat/expat-2.6.4/lib/*.c",
        "ThirdParty/Expat/expat-2.6.4/lib/*.h",
    ]),
    hdrs = [
        "ThirdParty/Expat/expat-2.6.4/lib/expat.h",
        "ThirdParty/Expat/expat-2.6.4/lib/expat_external.h",
        "ThirdParty/Expat/expat-2.6.4/lib/xmltok_impl.c",
        "ThirdParty/Expat/expat-2.6.4/lib/xmltok_ns.c",
    ],
    defines = ["XML_STATIC"] +
              select({
                  "@platforms//os:windows": [
                      "_WIN32",
                  ],
                  "@platforms//os:macos": [
                      "HAVE_ARC4RANDOM_BUF",
                  ],
                  "@platforms//os:linux": [
                    #   "HAVE_GETRANDOM",
                    "XML_POOR_ENTROPY"
                  ],
              }),
    includes = ["ThirdParty/Expat/expat-2.6.4/lib"],
    deps = [":expat_config"],
)

# C99 SNPRINTF
cc_library(
    name = "c99_snprintf",
    srcs = ["ThirdParty/c99_snprintf/c99-snprintf_1.1/snprintf.c"],
    hdrs = ["ThirdParty/c99_snprintf/c99-snprintf_1.1/system.h"],
    copts = [
        "-Wno-implicit-function-declaration",
        "-Wno-implicit-const-int-float-conversion",
    ],
    defines = [
        "HAVE_STDARG_H",
        "HAVE_STDLIB_H",
    ],
)

# FMI STANDARD HEADERS
cc_library(
    name = "fmi_headers",
    hdrs = glob(["ThirdParty/FMI/default/**/*.h"]),
    includes = [
        "ThirdParty/FMI/default",
        "ThirdParty/FMI/default/FMI1",
        "ThirdParty/FMI/default/FMI2",
        "ThirdParty/FMI/default/FMI3",
    ],
)

# -------------------------------------------------------------------------
# FMI Library
# -------------------------------------------------------------------------
cc_library(
    name = "fmilib",
    srcs = glob(
        [
            "src/CAPI/src/**/*.c",
            "src/Import/src/**/*.c",
            "src/Util/src/**/*.c",
            "src/XML/src/**/*.c",
            "src/ZIP/src/**/*.c",
            "src/XML/src-gen/**/*.c",
        ],
        # Exclude the unused FMIX query files that rely on missing headers
        exclude = [
            "src/XML/src/FMI1/fmi1_xml_query.c",
            "src/XML/src/FMI2/fmi2_xml_query.c",
            "src/XML/src/FMI3/fmi3_xml_query.c",
        ],
    ),
    hdrs = glob([
        "src/CAPI/include/**/*.h",
        "src/Import/include/**/*.h",
        "src/Util/include/**/*.h",
        "src/XML/include/**/*.h",
        "src/ZIP/include/**/*.h",
        "src/CAPI/src/**/*.h",
        "src/Import/src/**/*.h",
        "src/Util/src/**/*.h",
        "src/XML/src/**/*.h",
        "src/XML/src-gen/**/*.h",
    ]) + ["Config.cmake/fmilib.h"],
    copts = [
        "-DFMILIB_BUILDING_LIBRARY",
        "-DFMI_IA_LOG_DEBUG",
        "-Wno-unused-function",
        # Suppress the enum conversion warnings seen in your cmake log
        "-Wno-enum-conversion",
    ],
    includes = [
        "src/CAPI/include",
        "src/Import/include",
        "src/Util/include",
        "src/XML/include",
        "src/ZIP/include",
        "src/Util/include/JM",
        "src/Util/include/FMI",
        "src/XML/include/FMI",
        # Internal includes
        "src/Util/src/JM",
        "src/Util/src/FMI",
        "src/XML/src/FMI",
        "src/XML/src/FMI1",
        "src/XML/src/FMI2",
        "src/XML/src/FMI3",
        "src/XML/src-gen/FMI1",
        "src/XML/src-gen/FMI2",
        "src/XML/src-gen/FMI3",
        "src/CAPI/src",
        "src/Import/src",
        "src/Util/src",
        "src/XML/src",
        "src/ZIP/src",
        "Config.cmake"
    ],
    deps = [
        ":c99_snprintf",
        ":config",
        ":expat",
        ":fmi_headers",
        ":minizip",
        ":zlib",
    ],
)
