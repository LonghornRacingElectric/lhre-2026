load("@rules_cc//cc:defs.bzl", "cc_library")

filegroup(
    name = "hal_srcs",
    srcs = glob([
        "STM32G4xx_HAL_Driver/Src/**/*.c",
    ],         
    exclude = [
            # don't export templates (firmware provides its own)
            "STM32G4xx_HAL_Driver/Src/*template.c",
        ],),
    visibility = ["//visibility:public"],
)

# Makes the HAL available to consumers, the include paths will automatically
# be passed to the compiler
cc_library(
    name = "stm32_headers",
    hdrs = glob([
        "STM32G4xx_HAL_Driver/Inc/**/*.h",
        "STM32G4xx_HAL_Driver/Inc/Legacy/**/*.h",
        "CMSIS/Device/ST/STM32G4xx/Include/**/*.h",
        "CMSIS/Include/**/*.h",
    ]),

    includes = [
        "STM32G4xx_HAL_Driver/Inc",
        "STM32G4xx_HAL_Driver/Inc/Legacy",
        "CMSIS/Device/ST/STM32G4xx/Include",
        "CMSIS/Include",
    ],
    visibility = ["//visibility:public"],
)

# FreeRTOS Library
cc_library(
    name = "freertos_headers",
    hdrs = glob([
        "FreeRTOS/include/**/*.h",
        "FreeRTOS/portable/GCC/ARM_CM4F/**/*.h",
        "FreeRTOS/CMSIS_RTOS_V2/*.h",
    ]),

    includes = [
        "FreeRTOS/include",
        "FreeRTOS/CMSIS_RTOS_V2",
        "FreeRTOS/portable/GCC/ARM_CM4F",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "freertos_srcs",
    srcs = [
        "FreeRTOS/CMSIS_RTOS_V2/cmsis_os2.c",
        "FreeRTOS/portable/MemMang/heap_4.c",
        "FreeRTOS/portable/GCC/ARM_CM4F/port.c",
        ] + glob([
        "FreeRTOS/*.c",
        ],         
    ),
    visibility = ["//visibility:public"],
)

# USB_Device Library
cc_library(
    name = "usb_device_headers",
    hdrs = glob([
        "STM32_USB_Device_Library/Core/Inc/**/*.h",
        "STM32_USB_Device_Library/Class/CDC/**/*.h",
        "USB_Device/App/**/*.h",
        "USB_Device/Target/**/*.h",
    ]),

    includes = [
        "STM32_USB_Device_Library/Core/Inc",
        "STM32_USB_Device_Library/Class/CDC/Inc",
        "USB_Device/App",
        "USB_Device/Target",
    ],

    visibility = ["//visibility:public"],
)

filegroup (
    name = "usb_device_srcs",
    srcs = glob([
        "USB_Device/**/*.c",
        "STM32_USB_Device_Library/Core/**/*.c",
        "STM32_USB_Device_Library/Class/CDC/**/*.c",
        ],
    exclude = [
        "**/*template.c"
        ]
    ),

    visibility = ["//visibility:public"],
)