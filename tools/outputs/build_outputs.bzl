"""Macro for creating binary and hex files using objcopy."""

load("@aspect_bazel_lib//lib:transitions.bzl", "platform_transition_filegroup")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_python//python:defs.bzl", "py_binary")

def firmware_outputs(name, src, project_name, visibility = None, **kwargs):
    """
    Runs objcopy to convert a source file into both .bin and .hex files.

    Args:
      name: The name of the output target.
      src: The label of the single source file to convert.
      project_name: The name of the project.
      visibility: visibility of the target,
      **kwargs: extra args to pass to genrule.
    """

    # Define the output filenames based on the rule's name
    bin_out = project_name + ".bin"
    hex_out = project_name + ".hex"
    elf_out = project_name + ".elf"

    command = ("$(execpath @arm_none_eabi//:objcopy) -O binary $< $(location %s) && " +
               "$(execpath @arm_none_eabi//:objcopy) -O ihex $< $(location %s) &&" +
               "cp $< $(location %s)") % (bin_out, hex_out, elf_out)

    native.genrule(
        name = name,
        srcs = [src],
        outs = [
            bin_out,
            hex_out,
            elf_out,
        ],
        cmd = command,
        tools = ["@arm_none_eabi//:objcopy"],
        visibility = visibility,
        **kwargs
    )

def binary_out(name, src, visibility = None, **kwargs):
    """
    Runs objcopy to convert a source file (e.g., an ELF) into a raw binary.

    Args:
      name: The name of the output target. The output filename will be `name + ".bin"`.
      src: The label of the single source file to convert.
      visibility: The visibility of the generated rule.
      **kwargs: Additional arguments to pass to the underlying genrule.
    """
    native.genrule(
        name = name + "_bin",
        srcs = [src],
        outs = [name + ".bin"],
        cmd = "$(execpath @arm_none_eabi//:objcopy) -O binary $< $@",
        tools = ["@arm_none_eabi//:objcopy"],
        visibility = visibility,
        **kwargs
    )

def hex_out(name, src, visibility = None, **kwargs):
    """
    Runs objcopy to convert a source file (e.g., an ELF) into a hex binary.

    Args:
      name: The name of the output target. The output filename will be `name + ".hex"`.
      src: The label of the single source file to convert.
      visibility: The visibility of the generated rule.
      **kwargs: Additional arguments to pass to the underlying genrule.
    """
    native.genrule(
        name = name + "_hex",
        srcs = [src],
        outs = [name + ".hex"],
        cmd = "$(execpath @arm_none_eabi//:objcopy) -O ihex $< $@",
        # cmd_bat = "copy \"$(location @arm_none_eabi//:objcopy)\" objcopy.exe && objcopy.exe -O ihex $< $@",
        tools = ["@arm_none_eabi//:objcopy"],
        visibility = visibility,
        **kwargs
    )

def elf_out(name, src, visibility = None, **kwargs):
    """
    Copies input elf to an elf output.

    Args:
      name: The name of the output target. The output filename will be `name + ".elf"`.
      src: The label of the single source file to convert.
      visibility: The visibility of the generated rule.
      **kwargs: Additional arguments to pass to the underlying genrule.
    """
    native.genrule(
        name = name + "_elf",
        srcs = [src],
        outs = [name + ".elf"],
        cmd = "cp $< $@",
        cmd_bat = "copy $< $@",
        tools = ["@arm_none_eabi//:objcopy"],
        visibility = visibility,
        **kwargs
    )

MCU_FLAGS = [
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-fdiagnostics-color",
]

def firmware_project_g4(
        name,
        linker_script,
        startup_script,
        enable_usb = False,
        defines = [],
        extra_srcs = [],
        extra_deps = [],
        usb_device_name = None,
        extra_includes = [],
        enable_freertos = False,
        enable_dfu = False,
        locations = [],
        **kwargs):
    """Creates a firmware project for the STM32G4 family of chips.

    Args:
        name (string): name of the project
        linker_script (path): the location of the linker script being used (.ld file)
        startup_script (path): the location of the startup script being used (.s file)
        enable_usb (bool, optional): Whether or not to use USB drivers. Defaults to False.
        defines (list, optional): defines to pass to the compiler. Defaults to [].
        extra_srcs (list, optional): extra sources to compile with. Defaults to [].
        extra_deps (list, optional): extra dependencies to compile with. Defaults to [].
        usb_device_name (_type_, optional): name you want the USB driver to have. Defaults to None.
        extra_includes (list, optional): extra include paths to compile with. Defaults to [].
        enable_freertos (bool, optional): Whether or not to use FreeRTOS. Defaults to False.
        enable_dfu (bool, optional): Whether or not to accept strings to go into DFU. Defaults to False.
        locations (list, optional): A list of location identifiers (e.g., ["FR", "FL"]).
                                    For each location, a separate binary will be generated
                                    with a "BOARD_<location>" define. Defaults to [].
        **kwargs: extra args to pass to cc_binary.
    """

    # --- Base configuration that applies to all variants ---
    if (usb_device_name == None):
        usb_device_name = name

    final_extra_srcs = extra_srcs[:]
    final_extra_deps = extra_deps[:]
    final_defines = defines[:]

    if (enable_usb):
        final_extra_srcs.append("//drivers/stm32g4:usb_device_srcs")
        final_extra_deps.append("//drivers/stm32g4:usb_device_headers")
        final_defines.append('USB_DEVICE_NAME_TOKEN="ELC ' + usb_device_name + '"')

    if (enable_freertos):
        final_extra_srcs.append("//drivers/stm32g4:freertos_srcs")
        final_extra_deps.append("//drivers/stm32g4:freertos_headers")

    # enable DFU if the board has it enabled
    if (enable_dfu):
        final_defines.append("ENABLE_DFU")

    # --- Target generation logic ---
    release_srcs = []

    # If no locations are specified, run the original logic once.
    if not locations:
        locations_to_build = [None]
    else:
        locations_to_build = locations

    for location in locations_to_build:
        target_name = name
        project_name = name
        location_defines = []

        # If a location is specified, modify names and add the define
        if location:
            target_name = name + "_" + location
            project_name = name + "_" + location
            location_defines.append("BOARD_" + location)

        # Longhorn Lib is defined for both bare metal and FreeRTOS targets
        ll_version = "//drivers/longhorn-lib:longhorn_lib_stm32g4" if enable_freertos else "//drivers/longhorn-lib:longhorn_lib_base_stm32g4"

        # Main cc_binary target for the elf file
        cc_binary(
            name = target_name + "_project",
            srcs = native.glob([
                       "Core/Src/**/*.c",
                       "Core/Inc/**/*.h",
                   ], allow_empty = True) +
                   [
                       # Include the HAL for compilation
                       "//drivers/stm32g4:hal_srcs",
                   ] + final_extra_srcs,
            includes = [
                "Core/Inc",
            ] + extra_includes,
            deps = final_extra_deps + [
                "//drivers/stm32g4:stm32_headers",
                ll_version,
            ],
            linkopts = MCU_FLAGS + [
                "-Wl,-Map=" + target_name + ".map,--cref",  # Use unique map file
                "-Wl,--gc-sections",
                "-T $(location " + linker_script + ")",
                "$(location " + startup_script + ")",
                "-specs=nano.specs",
                "-lnosys",
                "-lc",
                "-lm",
                "-u _printf_float",
            ],
            defines = final_defines + location_defines + ["USE_HAL_DRIVER"],
            additional_linker_inputs = [
                linker_script,
                startup_script,
            ],
            target_compatible_with = [
                "@platforms//cpu:arm",
                "@platforms//os:none",
            ],
            copts = MCU_FLAGS + [
                "-mthumb-interwork",
                "-ffunction-sections",
                "-fdata-sections",
                "-Og",
                "-g3",
            ],
            visibility = ["//visibility:private"],
            features = ["generate_linkmap"],
            tags = ["stm32_firmware"],
            **kwargs
        )

        # Filegroup for the linkmap
        native.filegroup(
            name = target_name + ".out.map",
            srcs = [":" + target_name + "_project"],
            output_group = "linkmap",
            tags = ["stm32_firmware"],
        )

        # Platform transition to get the correct toolchain
        platform_transition_filegroup(
            name = target_name,
            srcs = [target_name + "_project"],
            target_platform = "//:arm_none_eabi",
            visibility = ["//visibility:public"],
            tags = ["stm32_firmware"],
        )

        elf_out(
            name = project_name,
            src = target_name,
            visibility = ["//visibility:public"],
            # tags = ["stm32_firmware"],
        )
        hex_out(
            name = project_name,
            src = target_name,
            visibility = ["//visibility:public"],
            # tags = ["stm32_firmware"],
        )
        binary_out(
            name = project_name,
            src = target_name,
            visibility = ["//visibility:public"],
            # tags = ["stm32_firmware"],
        )

        release_srcs.append(target_name + ".elf")
        release_srcs.append(target_name + ".bin")
        release_srcs.append(target_name + ".hex")

        py_binary(
            name = ("openocd_" + location) if location else "openocd",
            srcs = ["//tools/openocd:openocd_flashing_script"],
            main = "flash.py",
            data = [
                ":" + target_name + ".elf",
                "@openocd//:openocd",
                "//tools/openocd:g4_flashing_cfg",
            ],
            args = [
                "$(rlocationpath @openocd//:openocd)",
                "$(rlocationpath :" + target_name + ".elf" + ")",
                "$(rlocationpath //tools/openocd:g4_flashing_cfg)",
            ],
            deps = [
                "@rules_python//python/runfiles",
            ],
            tags = ["local", "flasher"],
        )

        py_binary(
            name = ("dfu_" + location) if location else "dfu",
            srcs = ["//tools/dfu:dfu_flashing_script"],
            main = "flash.py",
            data = [
                ":" + target_name + ".bin",
                "@dfu//:dfu",
            ],
            args = [
                "$(rlocationpath @dfu//:dfu)",
                "$(rlocationpath :" + target_name + ".bin" + ")",
            ],
            deps = [
                "@rules_python//python/runfiles",
                "@dfu_reqs//pyserial",
            ],
            tags = ["local", "flasher"],
        )

    native.filegroup(
        name = "release",
        srcs = release_srcs,
        visibility = ["//visibility:public"],
        tags = ["stm32_firmware"],
    )
