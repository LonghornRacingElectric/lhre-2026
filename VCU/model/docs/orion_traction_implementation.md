# Orion traction-control implementation

Branch: `codex/orion-traction-implementation`. Base main commit:
`d981057b8830f5f8be9c8b70a206dd2345748cd6`.

This is an integrated development implementation, with host-tested estimation,
control and CAN adapters. It is **not commissioned vehicle firmware**. Default
VCU mode is SHADOW; wheel calibration, acquisition timing, clock qualification
and vehicle-motion qualification remain false. No car was flashed. The clone
contains public GitHub main, not uncommitted changes from the owner's Mac.

## Confirmed geometry and phase observer

Four Hall sensors are spaced 15 mechanical degrees on a three-pole-pair ring.
Pairs 0/2 and 1/3 are quadrature; their origins differ by 45 electrical degrees.
`USM/firmware/usm/wheel_phase.c` normalizes measured channel offset, amplitude
and polarity, evaluates each pair with atan2, aligns the second pair, and unwraps
electrical phase. An alpha/beta observer estimates signed mechanical rad/s.
Electrical speed is divided by three; relative angle is not absolute hub angle.

The estimator checks both pairs, field magnitude, innovation, physical speed,
source freshness, sample ordering, age, acquisition gaps and channel skew. Any
rejection invalidates speed and starts reacquisition. Default acquisition is
10 ms and eight samples; this is a tuning choice, not demonstrated launch latency.
Pair consistency cannot detect a common measurement error or an unobserved full
turn. Physical maximum speed and acquisition bounds must make unwrap unambiguous.

`wheel_calibration.h` contains the commissioning gates and coefficient factory.
Populate it per corner from installed measurements; calibration=true alone cannot
make asynchronous acquisition suitable for control.

## USM firmware integration

- MLX90395 SPI mode 3 at 562.5 kHz for the current 144 MHz peripheral clock.
  This remains below the sensor's 1 MHz maximum. All SPI devices are deselected
  before starting burst acquisition. CubeMX source and `.ioc` agree.
- Full RM response: status, CRC, X/Y/Z/T/V. CRC-8 polynomial 0x07 checks the
  ten data bytes. Burst mode, error flags, DRDY, reset and the three-bit
  measurement counter qualify the acquisition. Repeated counters are rejected;
  an exact eight-conversion gap also cannot prove freshness.
- Bounded SPI timeouts replace indefinite waits. Burst startup transfer/status
  failure prevents that sensor from producing valid phase samples.
- Raw radial fields feed the phase observer. The existing filtered threshold
  speed remains in legacy telemetry; dedicated control packets carry phase speed.
- A nominal 1 ms task release uses an absolute schedule. Removed recurring USB
  field dumps from that task. This does not establish a 1 kHz fresh conversion
  rate, and does not claim 2 kHz acquisition.
- Enabled FDCAN2 RX interrupts at FreeRTOS-compatible priority 5 on both USM
  and VCU, added the missing VCU DAQ handler, and reserved standard filters
  for wheel/sync and existing critical-bus receive registrations.

**Acquisition work still required:** four 13-byte SPI exchanges take approximately
740 us wire time. Read-completion timestamps span approximately 555 us before
software/preemption overhead; default observer channel-skew limit is 100 us.
Actual burst conversion epochs need DRDY capture or another characterized scheme.
A common read-completion age correction cannot establish four conversion epochs.
The existing driver therefore cannot become qualified merely by enabling its
calibration flag. Configure/read back sensor conversion settings, measure fresh
conversion rates, determine per-channel phase error, and meet the timing bound
before setting `ORION_WHEEL_ACQUISITION_TIMING_QUALIFIED`.

## CAN contract and timing

| ID | Producer | Period | Eight-byte payload |
|---|---|---|---|
| 0x150–0x153 | FL/FR/RL/RR USM | nominal 3 ms | signed speed int16 at 0.01 rad/s; uint32 estimate epoch us; uint8 sequence; uint8 status |
| 0x154 | VCU | 100 ms | uint32 VCU pack epoch us; uint16 sequence; uint8 version; uint8 qualified |
| 0x155 | VCU | 20 ms | candidate Nm uint16 at 0.1; reference m/s uint16 at 0.01; worst rear slip m/s int16 at 0.01; state uint8; fault bits uint8 |

Status flags are defined in `drivers/longhorn-lib/traction_status.h`: VALID,
SYNC, DIRECTION, FAULT and protocol version. A mapped epoch is meaningful only
with SYNC set. USM publishes invalid speed as zero with validity clear.
All new control frames derive from the canonical CSV and generated CAN library.
They currently have no protobuf mappings: capture/decode raw CAN for commissioning.

Local microseconds derive from DWT with wrap-safe arithmetic. Fixed MCU clock
and calls more frequent than one cycle-counter wrap are prerequisites. A stopped
clock, long suspension or clock-rate change requires restart/reacquisition.

The clock-sync scaffold estimates offset from VCU pack time and USM software
FIFO reception. It **includes** arbitration, TX queue, receive FIFO and ISR delay;
it is not a hardware time synchronization guarantee. Both source acquisition
qualification and `ORION_TRACTION_CLOCK_QUALIFIED` default false. Qualify bounded
transport error/drift under load, or replace with a stronger synchronization
scheme before relying on mapped epochs. Counters alone do not align clocks.

Shared CAN RX now attaches FIFO-drain reception timestamps and DLC to the queued
payload. Freshness advances only after successful decoding. Queue drops,
undersized frames and failed decoding do not refresh stale data. RX decoding and
traction snapshots use matching short critical sections; TX phase/torque/TC
telemetry packing takes coherent snapshots too. The VCU extends the eight-bit
wheel counter and rejects backwards jumps. A USM reboot can require firmware
adapter restart; a park cycle alone does not reset the adapter counter history.

Declared bus traffic is approximately 44.5% DAQ and 52.0% critical at 1 Mbit/s,
using the existing generator's nominal calculation. This is not a measured or
worst-case stuffed-bit bound. Existing combined corner telemetry is retained.
Measure contention, queue delay, drops and worst-case control-message age; reduce
legacy traffic if necessary. The shared TX FIFO wait remains potentially blocking.

## VCU torque control

Order: pedal/brake and battery processing → torque map/derating → power limit →
PRNDL qualification → traction ceiling → existing regen/linelock → final state
and fault arbitration. Traction can reduce positive propulsion torque only.
Power limiting now starts from derated torque, preventing derating bypass.
Hydraulic release pulses use upstream driver-request torque, independently of
traction intervention.

The model supports DISABLED, SHADOW and ACTIVE. SHADOW calculates candidate
behavior while preserving the existing positive torque path. In the supplied
uncalibrated firmware it reports configuration inhibition rather than inventing
vehicle geometry/gains. Candidate/state/fault/slip telemetry is available on 0x155.

With qualified configuration and inputs:

1. Estimate rear-axle longitudinal reference speed from both front wheel speeds,
   road-wheel angles, yaw rate, wheelbase, track and effective rolling radii.
2. Correct each rear ground speed for yaw and compare it individually with its
   measured driven wheel speed. The worse rear slip constrains the single motor.
3. Blend a low-speed slip-velocity target into a speed-dependent slip target,
   avoiding division by nearly zero vehicle speed.
4. Use signed PI reduction with antiwindup, immediate reduction and bounded
   torque recovery. Enforce driver demand, launch envelope and motor torque cap.
5. Apply launch feedforward as a calibrated initial torque and time ramp, ending
   at a configured vehicle speed. This anticipates demand; it cannot sense slip
   before the measurements establish it and is not an identified grip model.
6. Reject stale/unsynchronized/wrapped-backwards/future data, excessive skew,
   invalid motion/geometry, reverse operation and excessive task dt. ACTIVE
   faults reduce to a configured conservative ceiling with bounded recovery.

A fresh control-output lease is checked by the CAN packer independently of
ControlTask. Initially, after 9 ms without an update, outgoing torque is zero,
limit zero and enable false. This catches a stalled control task even if CAN
continues retransmitting. It does not catch an entire CPU/CAN-task stall or
remove commands already queued. Validate inverter timeout, slew, queue residence,
external shutdown and an independent watchdog before activation.

Steering ADC conversion no longer blocks the 3 ms task; it consumes completed
conversions. The legacy steering-column conversion remains telemetry and is
**not** qualified road-wheel angle. No calibrated yaw/steering adapter is supplied:
`motion_valid` and `geometry_valid` remain false. Corner IMU data are not silently
used as body yaw or acceleration. Audit IMU register/scaling and transfer validity,
transform frames, compensate mounting motion, then validate any fusion design.
The current kinematic reference assumes negligible lateral velocity at the rear
axle; it is not robust to arbitrary sideslip or simultaneous front-wheel lock.

## Validation and next commissioning steps

Host results:

- All 46 existing VCU GoogleTests passed, including model and regen/linelock.
- Power-limit regressions: six scenarios plus driver-release bound passed.
- TC scenarios cover modes, launch, rolling start, rear slip, driver release,
  bounded recovery, faults, timing/sequence wraps, geometry and prolonged saturation.
- Wheel observer tests pass with address/undefined-behavior sanitizers, including
  calibrated waveforms, stationary noise, acceleration/reversal, aliasing,
  disagreement, invalid samples, timing faults and reacquisition.
- MLX manufacturer CRC vector, single-bit corruption and status checks pass.
- All ten existing/extended CAN GoogleTests passed.
- Real CAN base/RTOS host harness passes deferred decode, dropped queues, malformed
  DLC, decoder failures, coherent freshness and clock-wrap tests.
- Real VCU adapter tests pass control lease expiry/recovery/wrap, RX snapshot race,
  sequence wrap and generated wire encoding. Changed firmware sources pass host
  syntax checks with repository HAL/CMSIS/FreeRTOS headers.

These are not ARM link/build, HIL, hardware response, launch-performance or
regulatory validation results. Bazel and arm-none-eabi-gcc were unavailable here.

Reproducible commands from repo root:

```sh
sh VCU/model/tests/run_traction_control_tests.sh
python drivers/longhorn-lib/tests/run_can_rx_freshness_host_test.py
python VCU/firmware/tests/run_traction_adapter_host_test.py
bazel test //VCU/model/tests:all //USM/tests:all
```

Standalone USM compiler commands are in `USM/tests/README.md`. Existing VCU
GoogleTests were built manually against GoogleTest on this host.

Before active control: complete per-corner waveform/direction/radius calibration;
implement and qualify acquisition epochs and clock mapping; provide qualified
body motion and road-wheel geometry; identify drivetrain/inverter torque response;
populate TC limits/gains from controlled tests; validate fault injection and
shadow logs, then graduate from bounded low-torque tests to repeated launch and
corner-exit comparisons. Retain shadow mode until measurements justify enabling
intervention. This branch supplies the code foundation and explicit failure gates,
not evidence of an optimal tune or a measured lap-time gain.
