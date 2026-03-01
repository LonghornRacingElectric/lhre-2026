# FSAE Driverless 2026 — Track & Dynamic Events Reference

> Source: *2026 FSAE Driverless Supplement v1.0 (29 Sept 2025)* — sections DD and DI.2.
> Covers everything the autonomy software needs to know: cone specs, track geometry, event rules, penalties, and scoring.

---

## DD.1 — Track Marking

### Cone Color Convention

| Marker | Color | Stripe | Use |
|--------|-------|--------|-----|
| **Left boundary** | Small Blue | Single White | Track left edge |
| **Right boundary** | Small Yellow | Single Black | Track right edge |
| **Entry/Exit lanes** | Small Orange | Single White | Lane marking |
| **Start/Finish/Timing gates** | Large Orange | Dual White | Before and after lines |

### Cone Sizes

| Type | Dimensions (W x W x H) |
|------|------------------------|
| Small (blue, yellow, orange) | 228 mm x 228 mm x 325 mm |
| Large (orange) | 285 mm x 285 mm x 505 mm |

### Cone Part Numbers (WEMAS)

| Type | Part Number |
|------|-------------|
| Small Blue | 400.000043.00.00 |
| Small Yellow | 400.000013.01.10 |
| Small Orange | 400.000013.00.00 |
| Large Orange | 307.610500.00.00 |

### Marking Limitations (DD.1.2)

Expect real-world imperfections:
- Track lines may not be perfectly/continuously drawn
- Extra markings may exist (pavement lines, cone position marks, other event lines, surface color changes) — these are **not removed**
- Spare cones near the track or cones from other events at distinguishable distance
- Timekeeping equipment may be adjacent to track
- **No artificial landmarks** provided
- **No map data** given
- Teams **must not** place additional landmarks, beacons, or equipment on/inside the Dynamic Area

---

## DD.2 — Dynamic Penalties

| Penalty | Definition |
|---------|-----------|
| **DOO** (Down or Out) | Cone knocked down or displaced per D.8.1.1. Not replaced during run; missing cones don't cause rerun. |
| **OC** (Off Course) | All four wheels outside track boundary |
| **USS** (Unsafe Stop) | Vehicle fails to: stop in specified area, **or** reach Finished status within 30 sec of stopping |

---

## DD.3 — Dynamic Operations

### Startup Procedure (DD.3.1)

1. Team members move vehicle to starting location
2. Vehicle staged with steering straight ahead
3. **No additional equipment** permitted at staging/start line (no laptop, jack, pressure tank, etc.)
4. DSMS switched on by DSO only after official approval
5. Vehicle must enter **Ready** within **1 min** of staging or may be sent back

### Driving (DD.3.2)

- DSO at designated location with Remote Stop Control Box
- Vehicle enters Ready to Drive only via "Go" signal, after ≥ 5 sec in Ready state

### Run Completion (DD.3.3)

- Vehicle at standstill → 30 sec to attempt to continue
- After official approval, DSO deactivates via Remote Stop
- DSO + one team member collect vehicle immediately after official approval

---

## DD.4 — Dynamic Events

### Acceleration (DD.4.2)

| Parameter | Value |
|-----------|-------|
| Layout | Per D.9.1 + DD.1 |
| Staging | Foremost part 0.30 m from starting line |
| Attempts | Up to 4 runs |
| Finish | Full stop within **75 m** past finish line, in marked exit lane, go to Finished |

**Penalties:**
| Type | Penalty |
|------|---------|
| DOO | **+2 sec** per cone (incl. entry/exit gate cones) |
| OC | **DNF** for that run |
| USS | **DQ** for that run |

**Scoring (75 pts max):**
- **Starting Points** = 25 pts (vehicle crosses start line on ≥ 1 run)
- **Completion Points** = 25 pts (completes ≥ 1 run)
- **Performance Points** = 25 × (Teams Finishing + 1 - Place) / Teams Finishing
- Score = Starting + Completion + Performance

---

### Skidpad (DD.4.3)

| Parameter | Value |
|-----------|-------|
| Layout | Per D.10.1 + DD.1; 17 pylons around inside of each inner circle |
| Staging | Foremost part **15 m** from starting line |
| Attempts | Up to 4 runs |
| Finish | Full stop within **25 m** past finish line, in marked exit lane, go to Finished |

**Penalties:**
| Type | Penalty |
|------|---------|
| DOO | **+0.125 sec** per cone (incl. entry/exit gate cones) |
| OC | **DNF** for that run |
| Incorrect Laps | **DNF** (wrong count or wrong sequence) |
| USS | **DQ** for that run |

**Scoring (75 pts max):**
- Same structure as Acceleration (25 + 25 + 25)

---

### Autocross (DD.4.5)

| Parameter | Value |
|-----------|-------|
| Laps | **1 lap** per run |
| Attempts | Up to 4 runs |
| Finish | Full stop within **30 m** on track, go to Finished |

**Track Specifications (DD.4.4.1):**
| Constraint | Value |
|------------|-------|
| Max straight length | **80 m** |
| Min track width | **3 m** |
| Min turning diameter | **9 m** |
| Lap length | **200–500 m** |
| Features | Chicanes, multiple turns, decreasing radius turns, hairpin turns |

**Course Walk (DD.4.4.2):**
- At specified time before event
- Only non-electronic measurement devices (measuring wheel, tape measure)
- **No** antennas, sensors, cameras, GPS

**Staging:** Front wheels **6 m** behind starting line

**Penalties:**
| Type | Penalty |
|------|---------|
| DOO | **+2 sec** per cone (incl. cones after finish line) |
| OC | **+10 sec** per occurrence |
| USS | **DQ** for that run |

**Scoring (100 pts max):**
- **Starting Points** = 25 pts
- **Completion Points** = 25 pts
- **Performance Points** = 25 × (Teams Finishing + 1 - Place) / Teams Finishing
- *Note: 25 pts unaccounted in the formula vs 100 total — the remaining 25 likely from an additional category or the performance formula scaling differently. Verify against official score sheets.*

---

### Trackdrive (DD.4.6) — NOT RUN IN 2026

Included for future reference:
- 10 laps, vehicle must determine when run is complete (no external signal)
- Same track as Autocross
- Same penalties as Autocross
- Scoring TBD

---

## EBS Dynamic Test (DI.2)

This test is part of Technical Inspection, not a scored event.

| Parameter | Value |
|-----------|-------|
| Layout | Straight line marked with cones (similar to Acceleration) |
| Requirement | Accelerate in Driverless Mode to **≥ 40 km/h** within **20 m** |
| Trigger | Remote Stop at a specific point |
| Stopping distance | **≤ 8.5 m** |
| Wet conditions | Stopping distance may be scaled by officials |
| Timing | Done after other Technical Inspection items are completed |

---

## Key Numbers for Autonomy Software

Quick reference of values that directly parameterize the autonomy stack:

| Parameter | Value | Relevance |
|-----------|-------|-----------|
| Min track width | 3 m | Path planning corridor |
| Min turn diameter | 9 m → 4.5 m radius | Max curvature constraint |
| Max straight | 80 m | Speed planning horizon |
| Lap length | 200–500 m | Lap detection / mission planning |
| Small cone height | 325 mm | Perception / detection |
| Large cone height | 505 mm | Gate detection |
| Blue = left, Yellow = right | — | Cone classification |
| Orange = entry/exit/gates | — | Lane / timing detection |
| Staging distance (autocross) | 6 m behind start | Initial acceleration planning |
| Stop distance (autocross) | ≤ 30 m after finish | Braking planning |
| EBS reaction time | ≤ 200 ms | Safety system latency budget |
| EBS deceleration | > 10 m/s² | Emergency braking calibration |
| Ready → Go delay | ≥ 5 sec | Startup sequence timing |
| Standstill resume window | 30 sec | Recovery logic |
| Ready timeout at staging | 1 min | Startup reliability |
