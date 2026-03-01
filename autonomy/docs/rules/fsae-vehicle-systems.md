# FSAE Driverless 2026 — Vehicle Systems Reference

> Source: *2026 FSAE Driverless Supplement v1.0 (29 Sept 2025)* — sections DT and DO.
> Covers hardware integration, state machine, and safety systems.

---

## Definitions

| Term | Meaning |
|------|---------|
| **Driverless System** | Sensors + processing + actuators + SW/HW that lets the vehicle operate in Driverless Mode |
| **Manual Mode** | Operation by driver in the vehicle |
| **Driverless Mode** | Operation without a driver, Driverless System started |
| **DSO** | Driverless System Officer — responsible for all driverless operations at competition |

---

## DT.1 — Controls and Indicators

### DSMS — Driverless System Master Switch (DT.1.1)

- Must be direct acting (no relay or logic in between)
- Centered in a blue circle > 50 mm diameter, labeled "DS"
- Fitted with lockout/tagout in OFF position
- **When OFF:**
  - Driverless System **cannot** operate steering, braking, or propulsion
  - Sensors and processing **may** operate
  - Vehicle must be pushable and operable in Manual Mode

### Tractive System Activation Button (DT.1.2)

- Located near the Tractive System Master Switch
- Puts vehicle in Tractive System Active from outside the vehicle
- **Does nothing** when DSMS is OFF

### DSSI — Driverless System Status Indicators (DT.1.3)

Three indicators required:

| Location | Placement |
|----------|-----------|
| Left side | Behind cockpit, between 160 mm below Main Hoop top and 600 mm above ground |
| Right side | Same as left |
| Rear | On centerline, between 160 mm below Main Hoop top and 100 mm above Brake Light |

At least one DSSI must be visible from any angle at 1.6 m height within 3 m horizontal radius of Main Hoop.

Each DSSI:
- Black background
- Rectangular, triangular, or near-round, min 15 cm² shining surface
- LEDs without diffuser: max 20 mm apart; single LED line: min 150 mm length
- Shows **only** Driverless System Status (no other functions)

### Driverless Alert Sound (DT.1.4)

Triggered when status is **Emergency**:
- Intermittent tone, 1–5 Hz, 50% duty cycle
- 80–90 dBA (fast weighting) at 2 m radius
- Duration: 8–10 sec

---

## Driverless System Status — State Machine (DO.1.1)

| Status | DSSI Indication |
|--------|----------------|
| **Off** | Off |
| **Ready** | Yellow Continuous |
| **Driving** | Yellow Flashing |
| **Finished** | Blue Continuous |
| **Emergency** | Blue Flashing |

Transition logic (from the rules flowchart):
- Off → Ready: GLVMS and DSMS ON, EBS startup check passed, brake pressure built
- Ready → Driving: "Go" signal from Remote Stop Control Box, after ≥ 5 sec in Ready
- Driving → Finished: Mission complete, vehicle stopped
- Driving → Emergency: Shutdown circuit opened (EBS activated)
- Emergency triggers: DSB failure, EBS power loss, remote stop

---

## Driverless Missions (DO.1.2)

Required missions for 2026:
1. **Inspection** (DO.1.3)
2. **Manual Driving**
3. **Emergency Brake System Test**
4. **Acceleration**
5. **Skidpad**
6. **Autocross**
7. Trackdrive — *optional for 2026, expected in future years*

- Mission selection must **not** use an external device
- Mission indicator must show selected mission, be easily readable, located on dash or near DSMS

### Inspection Mission (DO.1.3)

Vehicle supported off ground, wheels removed:
1. Slowly spin the drivetrain
2. Slowly operate steering with a sine wave pattern
3. Transition to Finished after 25–30 sec

---

## DT.2 — Sensors and Actuators

### Steering Actuation (DT.2.1)

- Manual steering must work with DSMS Off — no extra steps required
- Steering actuation only possible when vehicle is Ready to Drive
- Steering may remain active during Emergency Brake Maneuver while moving

### Actuator Decoupling (DT.2.2)

Actuators may be disconnected for Manual Mode if:
- No parts removed for disconnection
- Mechanism doesn't block manual operation in any position
- Mechanism is securely locked in both positions

### Sensor/Camera Mounting (DT.2.3)

All driverless components must be inside:
- **(a)** Volume formed by Primary Structure, Tire Surface Envelope, outer tire surfaces, and chassis bottom plane
- **(b)** Permitted Aerodynamic Device locations (T.7.5, T.7.6, T.7.7)

---

## DT.3 — Brake Systems

### DSB — Driverless System Brake (DT.3.1)

- Tractive System is **not** a brake system
- All parts inside Rollover Protection Envelope
- Manual braking must always be possible
- DSB may be part of hydraulic brake system

**Deactivation Points** (max 2):
- Mounted per DT.2.3.a, near DSMS or on top of vehicle between Front Bulkhead and Front Hoop
- Near each other, protected against cone impacts
- Marked "Brake Release", red handle, operable without electrical power
- Max two push/pull/turn actions (order/direction shown next to points)

**Function:**
- Startup Check required before Ready status — verifies brake pressure buildup
- After startup check, DSB and signals must be continuously monitored for failures

### EBS — Emergency Brake System (DT.3.2)

The DSB must include an EBS.

**Technical:**
- Must use only passive systems with mechanical energy storage
- Directly supplied by GLVMS, DSMS, Remote Stop Relay, and EBS Relay (no delay)

**Emergency Brake Maneuver triggers:**
- DSB or signal failure
- Electrical power loss at EBS

**Safe State** (all three required):
- Vehicle at standstill
- Brakes engaged (no rolling)
- Shutdown circuit open

**Emergency Brake Maneuver performance (DT.3.2.5):**
- Reaction time (shutdown circuit open → deceleration start): **≤ 200 ms**
- Average deceleration: **> 10 m/s²** (dry conditions)
- Single failure: should achieve ≥ 50% performance
- Vehicle must remain in stable driving condition while decelerating

### Remote Stop System (DT.3.3)

Standard Remote Stop System with:
- Vehicle Module
- Control Box
- Remote Stop Relay

Control Box buttons:
- **Stop** → Vehicle Module opens Shutdown Circuit
- **Go** → Preselected Driverless Mission may start

Antenna: mounted unobstructed, no interfering parts nearby.

---

## DT.4 — Shutdown System

Shutdown Circuit for Driverless must include:

### Driverless System Relay (DT.4.1)
- Normally Open relay
- Closes Shutdown Circuit when DS checks complete
- Logic per DO.2.3.1:
  - **Manual Mode:** Manual mission selected, DS verified DSB deactivated
  - **Driverless Mode:** Driverless mission selected, DSMS ON, sufficient brake pressure (brakes applied)

### Remote Stop Relay (DT.4.2)
- In series with Shutdown Buttons
- Bypassed by normally closed relay in Manual Mode (relay directly supplied by DSMS with safety-certified forcibly guided or mirrored NO contact in series with DSMS)

### EBS Relay (DT.4.3)
- Fail-open switch — starts EBS when Shutdown Circuit opens
- Coil in parallel with Isolation Relays

---

## Teleoperation Rules (DO.2.2)

Between start line crossing and finish line crossing:
- **No wireless communication to the vehicle** permitted
- One-way telemetry **from** the vehicle is allowed
- Remote Stop Control Box is the **only** device that may send commands during any driverless operation
