# Autonomy Retrofit Roadmap — 2026–27 School Year

**Status:** Approved; repo snapshot (2026-07-15)
**Audience:** LHR autonomous systems members + team management
**Owner:** Autonomous Systems lead
**Note:** The live timeline is the VMS Timeline database in Notion. If they disagree, Notion wins.

---

## The Goal

By the end of the 2026–27 school year, retrofit the previous-year car with autonomous hardware and run a **fully functional autonomous driving demo** — not rules-compliant, but real: the car drives itself through at least one FSAE driverless event (ideally more) with no one in it.

**Why:** The current car (in design now) cannot risk unproven autonomy, and rules require competing on the current-year car — so the retrofit car will never compete. Its job is to prove the team's capability, so that autonomy is integrated directly into the **next** design cycle and we compete the year after. The end-of-year demo is the evidence for that decision.

**Definition of done (target, in descending priority):**
1. Autonomous acceleration run (straight-line launch, timed, autonomous stop)
2. Autonomous skidpad (figure-8 on known geometry)
3. Autonomous autocross-style cone loop (full perception, unknown track)

Achieving #1 alone is a successful year. #1 + #2 is a great year. All three means we're ahead of schedule.

---

## Where We Start (July 2026)

**Built and working (in simulation):** a complete ROS 2 pipeline — LiDAR cone detection, Delaunay-based track building, pure-pursuit control with speed planning, mission state machine, metrics — that completes laps in Gazebo physics sim with a simulated VLP-16. Merged to `main` (PRs #158, #172).

**Not built:** camera perception (designed, no code), state estimation (sim uses ground-truth odometry), any real hardware interface (no CAN bridge, no actuators, no compute target), safety systems. LiDAR perception is reliable on oval tracks, unreliable on autocross-style tracks.

**Key external dependency:** the retrofit car needs a new battery (owned by another subteam). The car likely cannot drive until spring semester. This roadmap treats the downtime as an asset — the drive-by-wire retrofit happens while the car is apart — but all on-track dates assume **battery back by mid-January**.

---

## Phase Overview

Phases overlap deliberately. Software and hardware run as parallel tracks all year — hardware procurement and drive-by-wire design **cannot** wait for software to finish, because lead times and fabrication dominate the schedule.

| # | Phase | Dates | One-liner |
|---|-------|-------|-----------|
| 0 | Planning & procurement | Jul 15 – Aug 23, 2026 | Decide everything; order everything |
| 1 | Sim completion & MVP framework | Aug 24 – Oct 23 | Close the sim gaps; freeze architecture; onboard new members |
| 2 | Testing suites & bench bring-up | Oct 26 – Dec 4 | CI + regression; real sensors on real compute; DBW mounted on the down car |
| — | Winter break (light work) | Dec 14 – Jan 10 | Optional: model training, sim tuning — nothing critical-path |
| 3 | Vehicle integration & first motion | Jan 11 – Mar 12, 2027 | Car powered; computer moves the car; teleop end-to-end |
| 4 | Closed-loop autonomy & demo | Mar 22 – May 1 | First autonomous meters → event demos → management demo |

**Academic anchors:** Fall classes Aug 24 – Dec 7 (Thanksgiving Nov 23–28, finals Dec 10–14). Spring classes Jan 11 – Apr 26 (spring break Mar 15–20, finals Apr 29 – May 3). Milestones are set on Fridays and avoid exam weeks.

---

## Milestone Calendar

| Date (Fri) | # | Milestone | Exit criterion |
|------------|---|-----------|----------------|
| **Aug 21, 2026** | M0 | Retrofit plan approved | Management signs off on this plan + budget; all long-lead hardware ordered |
| **Sep 11, 2026** | M1 | Architecture freeze | Interfaces, repo layout, and dev-environment docs stable; onboarding begins on solid ground |
| **Oct 23, 2026** | M2 | Sim v2 complete | Autocross lap in sim using **estimated** state (EKF, no ground truth) + camera cone classification in sim |
| **Dec 4, 2026** | M3 | Bench rig live | Full sensor suite + stack running in realtime on target compute, recording data; DBW actuators mounted on car |
| **Feb 12, 2027** | M4 | Car-on-stands control | Computer commands steering + brakes over CAN with car on stands; remote e-stop validated |
| **Feb 19, 2027** | — | Midyear management review | Show M4 footage; confirm demo scope and spring test plan |
| **Mar 12, 2027** | M5 | Teleop drive | Car driven at low speed by joystick through the full autonomy DBW path (perception stack logging live) |
| **Apr 2, 2027** | M6 | First autonomous run | Car completes a straight-line autonomous run, starts and stops itself |
| **Apr 23, 2027** | M7 | **Demo day** | Autonomous demo for team management — target: acceleration + skidpad; stretch: autocross loop |
| Apr 26 – May 1 | — | Buffer / re-run window | Slack for weather, breakage, or a second showing |

---

## Phase Details

### Phase 0 — Planning & Procurement (Jul 15 – Aug 23)

The most leveraged six weeks of the year. Every week of indecision here is a week subtracted from spring testing.

- **System architecture doc:** sensor suite (LiDAR model, camera(s), GNSS/INS), compute platform (e.g., Jetson-class vs. x86), power budget, mounting concept, network/CAN topology.
- **Drive-by-wire concept:** steering actuation (motor on column vs. rack), brake actuation (electromechanical actuator vs. hydraulic), throttle (already electric — VCU torque request), remote e-stop design. This is a mechanical + electrical design task and it starts **now**, not in the hardware phase.
- **Safety concept:** minimum viable safety for a non-compliant test car — hard-wired remote kill, geofenced test procedures, spotter protocol. Written and reviewed before any actuator is energized.
- **Bill of materials + budget** to management; identify sponsorship/discount routes (many sensor vendors have FSAE programs).
- **Recruiting plan** and onboarding curriculum outline for fall recruits.
- **Exit (M0, Aug 21):** plan + budget approved; every item with >4-week lead time is ordered.

### Phase 1 — Sim Completion & MVP Framework (Aug 24 – Oct 23)

Two goals in parallel: close the known simulation gaps, and make the codebase onboardable before new members arrive (~mid-September).

**Framework (front-loaded, done by M1 Sep 11):**
- Freeze topic contracts / message interfaces between subsystems.
- Dev-environment setup that a new member completes in one evening (docs + scripts already mostly exist).
- Starter tasks scoped for recruits (cone-detection tuning, sim scenarios, viz tools).

**Sim stack (the existing plan's Phases 2b/3, done by M2 Oct 23):**
- Fix LiDAR robustness on autocross (cone duplication during swerves — the known open bug).
- **State estimation:** EKF fusing wheel odometry + IMU (+ GNSS when hardware arrives) — replace ground-truth odom in sim. This is the single biggest software gap between sim and the real car.
- Camera perception per `camera-fusion.md`: color classification in sim; begin YOLO training on FSOCO for the real car.
- Implement skidpad + acceleration missions in the mission manager (currently stubbed) — these are the demo events.
- **Exit (M2, Oct 23):** full autocross lap in sim with no ground truth anywhere in the loop.

### Phase 2 — Testing Suites & Bench Bring-up (Oct 26 – Dec 4)

Hardware has arrived; the car is apart awaiting its battery. Everything that can be proven off-car gets proven off-car.

**Testing suites:**
- CI on the ROS 2 workspace: unit tests for perception/planning/control math, plus a **sim regression suite** — headless lap runs whose metrics (lap completion, cross-track error) gate merges.
- rosbag record/replay pipeline: every real-sensor session becomes a replayable test asset.

**Bench bring-up:**
- Target compute flashed, ROS 2 running, stack benchmarked in realtime on it (not on laptops).
- Sensor drivers live: LiDAR spinning on the bench, camera calibrated, GNSS/INS reporting.
- Sensor rig (cart or car-mounted, pushed/towed) collecting real outdoor pointclouds of actual cones — first contact between our detector and reality.

**DBW on the down car:**
- Steering + brake actuators fabricated, mounted, and bench-actuated (car unpowered, on stands).
- CAN interface design between autonomy compute and vehicle (VCU) — leverage the team's existing CAN tooling.
- **Exit (M3, Dec 4):** bench rig records the full sensor suite through the real stack on target compute; actuators are on the car.

### Winter Break (Dec 14 – Jan 10) — light, optional

YOLO/FSOCO training runs, sim tuning, documentation. Nothing on the critical path is scheduled here — assume zero output and be pleasantly surprised.

### Phase 3 — Vehicle Integration & First Motion (Jan 11 – Mar 12)

Assumes battery return ~mid-January (see Risks if not).

- **Weeks 1–3:** electrical integration and CAN bridge commissioning; sensors permanently mounted, calibrated (extrinsics), powered from the car.
- **Manual-drive data collection** as soon as the car drives at all: a human drives cone courses while the full stack runs open-loop — perception and state estimation validated against reality without risk.
- **On-stands actuation (M4, Feb 12):** computer steers and brakes the car on stands via CAN; remote e-stop kill path tested exhaustively (it must dominate every other command, every time).
- **Midyear management review (Feb 19):** show the footage, hold the coalition together, confirm demo scope.
- **Teleop (M5, Mar 12):** low-speed driving via joystick through the exact software/electrical path autonomy will use — proves every link except the planner. Scheduled the Friday before spring break.

### Phase 4 — Closed-Loop Autonomy & Demo (Mar 22 – May 1)

Weekly test days, minimum. Event order matches difficulty:

1. **Acceleration** (M6, Apr 2): straight line, minimal perception, speed capped low and raised gradually. First fully autonomous meters.
2. **Skidpad:** fixed known geometry — can run on a pre-surveyed map, exercising control and state estimation without full perception.
3. **Autocross loop (stretch):** unknown cone track, full perception pipeline — the sim stack's real graduation exam.

- **Demo day (M7, Apr 23):** rehearsed, filmed, management invited. Target acceleration + skidpad; autocross if it's ready.
- Apr 26 – May 1 held as weather/breakage buffer.

---

## Descope Ladder

If the schedule slips, cut from the top; the bottom is protected at all costs.

| Level | Scope | Cut trigger |
|-------|-------|-------------|
| Full | Autocross + skidpad + accel, camera+LiDAR fusion | — |
| −1 | Drop autocross; LiDAR-only perception (skip camera on real car) | M2 or M3 slips >3 weeks |
| −2 | Skidpad + acceleration only, pre-mapped | Teleop (M5) slips past spring break |
| Floor | **Autonomous acceleration run only** | Protect this even if everything else burns |

An autonomous straight-line run with an empty cockpit is still a management-convincing demo. Everything above it is upside.

## Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Battery slips past mid-Jan** | Compresses Phases 3–4 | All of Phase 2 is battery-independent by design; on-stands work (M4) needs LV power only — coordinate a bench LV supply; descope ladder |
| Procurement lead times / budget delay | Nothing to bench in Phase 2 | M0 hard-gates orders by Aug 21; pursue vendor FSAE programs; borrow/loaner units as fallback |
| DBW mechanical complexity underestimated | M4/M5 slip | Design starts in Phase 0; actuators bench-tested before car mounting; steering-only fallback (brake via e-stop + coast) for early runs |
| Small team + course load | Everything slips a little | Architecture freeze before onboarding; milestones avoid exam weeks; winter break carries zero critical path |
| Test-site access & safety signoff | Spring test cadence starves | Identify lot + get standing permission during fall; safety procedures written in Phase 0, reviewed before M4 |
| Sim-to-real perception gap | Detector fails on real pointclouds | Sensor-rig data collection starts in **November** (Phase 2), not spring — months of real data before it matters |

## Standing Cadence

- **Weekly:** subteam meeting; sim regression suite green on `main`.
- **Fall:** hardware track and software track report separately — they are parallel projects.
- **Spring:** weekly test day (car or bench), every session recorded to rosbag.
- **Per milestone:** short written status to management (photos/video > prose).
