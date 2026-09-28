# HVC CAN integration test firmware

This build is only for testing the sealed accumulator with the maintenance
plugs removed. It exercises the real shutdown loop, state-machine timing, AIR+
command, contactor sense inputs, and Orion CAN messages.

Do not use this image with the maintenance plugs installed. The image bypasses
the BMS trip output and permits the physical contactors to actuate.

## What the test image changes

- The state machine compares a simulated 100 V pack against a simulated 90 V
  tractive voltage, satisfying the normal 85% precharge threshold.
- BMS faults remain measured, latched, printed, and transmitted over CAN, but
  they do not block state transitions or assert the physical BMS trip output.
- Charger presence is forced false so the test follows the drive precharge
  states rather than the charging states.

The real eight-second startup inhibit, shutdown/AIR- sense, five-second
precharge qualification, AIR+ output, and both contactor sense inputs remain
active.

## Build and flash the test image

```sh
bazel build //legacy/2025/hvc:hvc-firmware-2025-can-test.hex
```

The output is:

```text
bazel-bin/legacy/2025/hvc/hvc-firmware-2025-can-test.hex
```

If flashing through the repository's DFU helper:

```sh
bazel run //legacy/2025/hvc:dfu_can-test
```

The USB terminal prints this warning once per diagnostics cycle:

```text
*** CAN TEST: SIMULATED PRECHARGE, BMS TRIP BYPASSED, CONTACTORS ACTIVE ***
```

It also prints a `CAR RX VCU_STATE` line once per second. The line shows packet
freshness and age plus PRNDL, STOMP, ready-to-drive buzzer, VCU state-of-charge
estimate, line lock, and event mode.

An `OK` status means the packet arrived within its Orion timeout. `TIMEOUT`
means the HVC is not currently receiving VCU state; values shown after a
timeout are stale and should not be interpreted as current car state.

## Expected state sequence

1. With the shutdown loop open, CAN `0x131` reports state 0 and AIR+ remains
   open.
2. Closing the shutdown loop causes state 1 and sets the precharge status bit.
3. After five continuous seconds, state 2 commands AIR+ closed.
4. Opening the shutdown loop returns to state 0 and commands AIR+ open.

Before testing, keep the maintenance plugs and charger disconnected and verify
that the accumulator output is near zero volts. Individual cell sections must
still be treated as live.

## Restore production firmware

The normal target never receives the test define. Rebuild and flash it:

```sh
bazel build //legacy/2025/hvc:hvc-firmware-2025.hex
bazel run //legacy/2025/hvc:dfu
```

After restoring production firmware, confirm that the CAN-test warning no
longer appears and that BMS faults once again inhibit precharge.
