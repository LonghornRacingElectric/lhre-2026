# ADBMS6830 source files - included directly in HVC firmware build

filegroup(
    name = "adbms_sources",
    srcs = [
        "src/adBms6830GenericType.c",
        "src/adBms6830ParseCreate.c",
        "src/adBms_Application.c",
        "src/mcuWrapper.c",
        "src/serialPrintResult.c",
    ] + glob(["inc/*.h"]),
    visibility = ["//HVC/firmware:__pkg__"],
)
