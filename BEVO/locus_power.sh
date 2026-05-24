#!/usr/bin/env bash
# Drive GPIO14 (BCM 14, header pin 8) HIGH to power the Locus Lock rail on
# BEVO Rev B. Uses `pinctrl` (raspi-utils) rather than lgpio because lgpio
# releases its claim on gpiochip_close, after which the kernel pinctrl
# reverts pin 14 to its device-tree default (input, pull-down) and the
# line drops LOW. `pinctrl set` writes the mux directly and persists until
# another driver reclaims it.
#
# Prereq: serial console MUST be disabled on this Pi. UART0 is the kernel's
# default owner of BCM 14/15, and even with `console=serial0` removed from
# cmdline.txt and serial-getty@ttyAMA10 disabled, you need both gone before
# pinctrl will hold. dashd/deploy/install.sh handles this idempotently.
set -euo pipefail

PIN=14

usage() {
    echo "usage: $0 {on|off|status}" >&2
    exit 1
}

case "${1:-}" in
    on)
        pinctrl set "$PIN" op dh
        echo "Locus Lock powered ON (GPIO${PIN} = output high)"
        ;;
    off)
        pinctrl set "$PIN" op dl
        echo "Locus Lock powered OFF (GPIO${PIN} = output low)"
        ;;
    status)
        pinctrl get "$PIN"
        ;;
    *)
        usage
        ;;
esac
