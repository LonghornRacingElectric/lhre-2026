import lgpio
import time
import argparse
import sys

# GPIO14 (BCM 14, header pin 8) drives the Locus Lock power enable on BEVO
# Rev B. HIGH = powered on. Without this, the Locus Lock board has no power
# even though it is "connected" over Ethernet, so cand/:2000 never sees any
# NMEA traffic.
#
# Note: BCM14 is UART0 TXD by default. If the serial console is enabled in
# /boot/firmware/cmdline.txt (console=serial0,115200) or config.txt
# (enable_uart=1), gpio_claim_output may fail with EBUSY. The Rev B setup
# disables the serial console; if you see a claim error here, that is the
# first thing to check.
LOCUS_POWER_EN = 14
CHIP = 0


def _open_chip():
    return lgpio.gpiochip_open(CHIP)


def check_status(h):
    try:
        lgpio.gpio_claim_input(h, LOCUS_POWER_EN)
        current_state = lgpio.gpio_read(h, LOCUS_POWER_EN)
        if current_state == 1:
            return True, "Locus Lock power rail is HIGH (on)"
        else:
            return False, "Locus Lock power rail is LOW (off)"
    except Exception as e:
        return None, f"Error reading GPIO{LOCUS_POWER_EN}: {e}"


def power_on(h):
    try:
        lgpio.gpio_claim_output(h, LOCUS_POWER_EN, 1)
        lgpio.gpio_write(h, LOCUS_POWER_EN, 1)
        time.sleep(1)
        print("Locus Lock powered ON")
    except Exception as e:
        print(f"POWER_ON ERROR: {e}")
        raise


def power_off(h):
    try:
        lgpio.gpio_claim_output(h, LOCUS_POWER_EN, 0)
        lgpio.gpio_write(h, LOCUS_POWER_EN, 0)
        time.sleep(1)
        print("Locus Lock powered OFF")
    except Exception as e:
        print(f"POWER_OFF ERROR: {e}")
        raise


def main():
    parser = argparse.ArgumentParser(description="Control Locus Lock power rail (GPIO14)")
    parser.add_argument(
        "action",
        choices=["on", "off", "status"],
        help='Action: "on" / "off" / "status"',
    )
    args = parser.parse_args()

    h = _open_chip()
    try:
        if args.action == "status":
            _, msg = check_status(h)
            print(msg)
        elif args.action == "on":
            is_on, _ = check_status(h)
            if is_on:
                print("Locus Lock already powered ON")
            else:
                power_on(h)
        elif args.action == "off":
            is_on, _ = check_status(h)
            if not is_on:
                print("Locus Lock already powered OFF")
            else:
                power_off(h)
    except KeyboardInterrupt:
        print("\nOperation cancelled")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
    finally:
        lgpio.gpiochip_close(h)


if __name__ == "__main__":
    main()
