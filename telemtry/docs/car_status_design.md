# Car Status — central state classification, history, and CSV tagging

**Status:** Design proposal (for review before implementation)
**Author:** (drafted with Claude Code)
**Scope:** A single authoritative source of truth for the car's high-level state
(Off / On-Idle / Ready-to-Drive / Moving / Fault), broadcast live, persisted as
queryable history, and used to tag exported CSV/MoTeC logs with motion windows.
Plus a master **Car Status** page showing live + historical state and LV/HV SoC.

---

## 1. Motivation

Today, car-state logic is **scattered and ephemeral**:

- The live UI re-derives bits of state ad hoc — e.g. `ShutdownScreen.tsx` shows
  `hvcStateMachine`, `contactorState`, `r2dStatus`, shutdown legs as raw chips,
  but nothing classifies an overall state.
- `kafkaConsumer.ts` (`buildNormalizedSensorRoutes`, ~L302) normalizes all the
  needed signals (contactors, r2d, shutdown legs, wheel speeds, `hv_soc`,
  `lvV`) but only to feed live tiles — none of it is persisted as "state".
- `CSV_to_DB.py` (`event_seperator`, ~L58–159) detects motion from wheel speed
  **only at ingest time**; the result is never stored. So "when was the car
  actually moving?" is not queryable.

**Goal:** one classifier, one definition, consumed by every tool. Decided
direction (see questions resolved with the team):

1. **Authoritative classification runs in a stack processor** (`car_status`),
   server-side and always-on — same pattern as `gps_classifier` / `lap_timer`.
2. **Energy readouts are HV SoC (real `hv_soc`) + LV battery voltage.** There is
   no LV state-of-charge signal (only `lv_batt_v`), so LV is shown as voltage —
   no estimated LV SoC.
3. **Self-contained, no drive-day dependency.** The `drive_day` / `classifier`
   tables are abandoned; car-status segments live in their own standalone table
   keyed by car + time.
4. This document is the **design-first** deliverable; implementation follows
   review.

---

## 2. Available signals (verified against the schema)

All fields below exist today. Names are the **decoded protobuf field names**
(snake_case in CAN CSV; the viewer decodes to camelCase). Sources:
`drivers/longhorn-lib/config/can_packets.csv`,
`drivers/longhorn-lib/protobuf/can_packets.proto` (Orion),
`telemtry/stack/ingest/protobuf/angelique.proto` (Angelique).

| Purpose | Field | Proto table | CAN id | Notes |
|---|---|---|---|---|
| HV+ contactor | `pos_hv_contactor` | DiagnosticsHigh | 0x131 | bool |
| HV− contactor | `neg_hv_contactor` | DiagnosticsHigh | 0x131 | bool |
| Precharge contactor | `precharge_contactor` | DiagnosticsHigh | 0x131 | bool |
| Contactor state (Angelique) | `contactor_state` | Pack | — | int32; Angelique uses this instead of the 3 bools |
| Ready-to-drive status | `r2d_status` | DiagnosticsLow | 0x120 | bool |
| Ready-to-drive authorized | `r2d_authorized` | DiagnosticsLow | 0x121 | bool |
| Shutdown legs | `shutdown_leg1..4` | DiagnosticsLow | 0x134 | bool ×4 |
| BMS comm error | `bmb_comm_error` | DiagnosticsLow | 0x134 | bool |
| IMD isolation error | `imd_gnd_isolation_error` | DiagnosticsLow | 0x134 | bool |
| Inverter POST faults | `post_faults` | DiagnosticsHigh | 0x0AB | uint32 bitmask |
| Inverter run faults | `run_faults` | DiagnosticsHigh | 0x0AB | uint32 bitmask |
| HV pack voltage | `hv_pack_v` | Pack | 0x132 | float, 0.01 V |
| HV pack current | `hv_c` | Pack | 0x132 | float, 0.01 A |
| **HV SoC** | `hv_soc` | Pack | 0x132 | float, 0.01 % — real signal, shown directly |
| **LV battery voltage** | `lv_batt_v` | Pack/PDU | 0x183 | float, 0.01 V — shown directly (no LV SoC) |
| LV battery current | `lv_batt_c` | Pack/PDU | 0x183 | float, 0.01 A |
| LV battery temp | `lv_batt_t` | Pack/PDU | 0x183 | float, 0.01 °C |
| Motor speed | `motor_speed` | Controls | 0x0A5 | float rpm |
| Inverter rpm | `inverter_rpm` | — | — | alt motor-rotation signal |
| Wheel speed (combined) | `wheel_speed` | Dynamics | 0x130 | int16, 2^-7 rad/s |
| Wheel speeds (per-corner) | `fl_wheel_speed`/`fr_wheel_speed`/`bl_wheel_speed`/`br_wheel_speed` | Dynamics | — | proto fields 9–12; viewer decodes to `flwSpeed`/etc. |
| GPS speed | `gps_speed` | Dynamics | — | proto field 3; viewer `gpsSpeed`, m/s |

> **Angelique vs Orion**: Angelique uses `contactor_state` (int) and `lv_v`/`lv_c`
> rather than the Orion bool contactors / `lv_batt_*`. The classifier reads via a
> small per-car field-accessor so one rule set works for both. The viewer already
> abstracts this in `kafkaConsumer.ts` (`firstDefined(pack, ["lvV","lvBattV"])`).

### Energy readouts: HV SoC + LV voltage
- **HV** uses the real `hv_soc` field, shown directly as a percentage.
- **LV** has **no state-of-charge signal** — only `lv_batt_v`. We show LV
  **battery voltage** directly (plus current/temp), no estimated SoC. (A
  voltage→SoC estimate can be added later if the team provides the LV pack
  chemistry/cell-count, but it is explicitly out of scope here.)

---

## 3. State model

**Four** mutually-exclusive states, evaluated **in priority order** (first match wins):

| State | Condition (conceptual) |
|---|---|
| **MOVING** | `|motor_speed|` > MOVE_RPM **or** mean wheel speed > MOVE_WHEEL **or** `gps_speed` > MOVE_MPS |
| **READY** | `r2d_status` true **and** all shutdown legs closed **and** HV live, but not moving |
| **ON_IDLE** | HV live (contactors closed / `hv_pack_v` > HV_LIVE_V) but not ready and not moving |
| **OFF** | HV not live (`hv_pack_v` < HV_LIVE_V and contactors open); LV may still be up |

**Faults are NOT a state** (team decision). They are computed every frame and
reported **separately** as an advisory `active_faults` list, so a fault can be
present in *any* of the four states without changing "what the car is doing":

| Fault reason | Condition |
|---|---|
| `run_faults` / `post_faults` | inverter fault bitmask nonzero |
| `imd_isolation` | `imd_gnd_isolation_error` |
| `bmb_comm_error` | `bmb_comm_error` |
| `shutdown_open` | a shutdown leg open while HV is live |

Notes:
- **Thresholds are config**, not magic numbers (see §6). MOVE_RPM, MOVE_WHEEL,
  MOVE_MPS, HV_LIVE_V, plus debounce timings.
- **Debounce / hysteresis**: a state must hold for `MIN_STATE_MS` (e.g. 500 ms)
  before it is committed, to avoid flapping on noisy frames. Brief dropouts
  shorter than `MAX_GAP_MS` do not end a segment.
- **`hvc_state_machine` is out of scope** (its integer enum is undocumented in
  the repo). Classification relies only on the unambiguous booleans (contactors,
  R2D, shutdown legs) and speed.
- **Per-car**: classification is identical; only field access differs.

A small ASCII view of the intended transitions (faults overlay, not shown):

```
   OFF ──► ON_IDLE ──► READY ──► MOVING
       ◄────────┴────────┴────────┘   (HV drop / r2d release / stop)
   active_faults[] is reported alongside whatever state is active.
```

---

## 4. Architecture (the "central" part)

```
                       ┌─────────────────────────────────────────────┐
 sensor_data (Kafka) ──►  car_status processor (NEW, always-on)       │
                       │   - decode Orion/Angelique protobuf          │
                       │   - classify each frame (state model §3)     │
                       │   - debounce → segment boundaries            │
                       └───────┬───────────────────────┬─────────────┘
                               │                        │
                  emits car_status topic         writes segments to DB
                  (live state transitions)       car_status_segment table (own)
                               │                        │
          ┌────────────────────┴───┐         ┌──────────┴───────────────────┐
          ▼                        ▼         ▼                              ▼
   Car Status page (live)   any consumer   Car Status page (history)   CSV/MoTeC
   via SSE bridge           (Grafana etc.) via DB query API            tagging at export
```

**Why a processor (not in the Next app):** it runs server-side regardless of
whether anyone has a browser open, so motion windows and state history are
captured for *every* session automatically — which is the whole point of "find
data later". It also means Grafana, the viewer, and future tools all read the
**same** classification instead of each re-deriving it.

### 4.1 The processor (`telemtry/stack/processors/car_status/`)
- Copy the `kafka_base` skeleton (`main.py` poll-loop + `Dockerfile` +
  `docker-compose.yml` + `requirements*.txt` + `BUILD.bazel`).
- **Consumes:** `sensor_data` (env `KAFKA_INPUT_TOPIC`, default `sensor_data`).
- **Decodes** by `car_type` header → `can_packets_pb2.OrionSensorData` /
  `angelique_pb2.AngeliqueSensorData` (same as `field_enricher`).
- **Classifies** via a pure function `classify(frame, prev_state) -> state`
  (unit-testable, no I/O) living in `car_status/classifier.py`.
- **Emits** `car_status` Kafka topic on every committed transition:
  `{ car, state, prev_state, t_ms, packet_id, hv_soc, lv_v, lv_c, hv_v,
     reasons: [...] }`. Also a periodic heartbeat with the current state so a
  late-joining live page gets the state immediately.
- **Persists** a row to the standalone `car_status_segment` table when a segment
  **closes** (see §4.2). Self-contained: keyed only by car + time/packet range,
  **no drive-day association**.
- **Registration:** add `car_status|processors/car_status` to `STACK_COMPONENTS`
  and to `ALL_ORDER` in `telemtry/stack/server_devtool.sh`; enable with
  `./server_devtool.sh enable car_status`. (Optional → not in CORE so it never
  blocks the core stack.)

### 4.2 Persistence — a standalone `car_status_segment` table
**The drive-day / `classifier` infrastructure is abandoned, so this feature does
not depend on it.** Car-status segments live in their own self-contained table,
keyed only by car and time/packet range — no `drive_day` FK, no `classifier`
reuse. One row per closed segment:

```sql
CREATE TABLE IF NOT EXISTS public.car_status_segment (
    id           bigserial PRIMARY KEY,
    car          text      NOT NULL,         -- 'orion' | 'angelique' | ...
    state        text      NOT NULL,         -- OFF | ON_IDLE | READY | MOVING | FAULT
    start_time   bigint    NOT NULL,         -- ms epoch (packet time)
    end_time     bigint,                     -- ms epoch; NULL only for the open/current segment
    start_packet bigint,                     -- packet_id at segment start (optional)
    end_packet   bigint,
    hv_soc_avg   real,                       -- representative HV SoC over the segment
    lv_v_avg     real,                       -- representative LV battery voltage
    reasons      text                        -- short JSON of the signals that drove the state
);
CREATE INDEX IF NOT EXISTS idx_css_car_time ON public.car_status_segment (car, start_time DESC);
CREATE INDEX IF NOT EXISTS idx_css_state     ON public.car_status_segment (state);
```

- **No migration coupling**: the `CREATE TABLE IF NOT EXISTS` is added to the
  ingest init SQL (or run by the processor on startup), independent of the
  drive-day tables.
- Writing uses the same SQLAlchemy/`QueryBuilder.insert` path `gps_classifier`
  uses — just against the new table/model instead of `Classifier`.
- Range queries are cheap thanks to `idx_css_car_time`; "moving windows" is just
  `WHERE state='MOVING'`.

This keeps the feature fully self-contained and queryable on its own terms,
which is exactly what the "find data later" goal needs.

### 4.3 Live delivery to the page
The viewer already has the Kafka→bus→SSE pattern (the trackside live bridge is a
model). A thin `/api/car-status/stream` SSE route subscribes to the `car_status`
topic on the **existing shared consumer/bus** and forwards state events — **no
new Kafka consumer**, same approach the merged trackside bridge uses. (No
dependency on `AppState`/`event-sync` or any drive-day state.)

### 4.3a Live-tunable thresholds (one authoritative classifier)
The right cutoffs (what rpm counts as "moving", what voltage counts as "HV live")
only become clear watching the real car, so thresholds are **adjustable live from
the UI** — without redeploying and without duplicating the classify logic in
TypeScript:

```
 /car-status sliders ──POST──► /api/car-status/config ──► car_status_config (Kafka)
                                                              │
                          car_status processor consumes it ───┘
                          → applies overrides to every car's state machine
                          → next frame reclassifies with new thresholds
                          → each car_status event ECHOES the active thresholds
 /car-status SSE ◄───────────────────────────────────────────┘  (UI shows effect live)
```

- The processor subscribes to **both** `sensor_data` and `car_status_config`. A
  config message is a partial `{key: number}` map; unknown/non-numeric keys are
  ignored (`Thresholds.from_overrides`), so a bad payload can never crash it.
- Because the **single Python classifier** stays authoritative, there is no TS
  reimplementation to drift. The UI only *sends* thresholds and *displays* the
  ones echoed back — it never classifies.
- Defaults live in `Thresholds` (processor) and are surfaced on the page; sliders
  seed from the values the processor echoes, so the UI always reflects reality.

### 4.4 History + "moving windows" query API
`/api/car-status/history?car=orion&from=<ms>&to=<ms>` reads
`car_status_segment` → returns ordered segments with durations, plus a derived
`movingWindows[]` (all MOVING segments) and totals (e.g. "moved 41 min across 7
windows"). Queried purely by **car + time range** (and optionally `state`), with
no drive-day concept. This is what makes "find the data where the car was
actually moving" a one-call lookup. Recent-activity views just default the range
to the last N hours/days.

### 4.5 CSV / MoTeC tagging
At export time (`src/lib/motec/exporter.ts` / `datalog.ts`), query the
car-status segments overlapping the export range and:
- add per-segment markers/laps to the MoTeC `.ldx` (it already supports
  segment metadata for laps), and/or
- emit a sidecar `*.status.json` alongside CSV exports describing the state
  timeline. This lets MoTeC and downstream tooling jump straight to motion.
Phase-2 nicety: bake state as a derived channel column in the CSV.

---

## 5. The Car Status page (`/car-status`)

A master page, live + historical, mounted client-only (same `ssr:false` pattern
the trackside page uses, since it reads localStorage / live streams).

> **Build gotcha** (learned from trackside #279): the viewer's prod build runs
> `next build`, where `react/no-unescaped-entities` is a **build-blocking error**
> that `tsc` does **not** catch — a stray `'`/`"` in JSX text broke the prod
> build after #273. So: escape apostrophes/quotes in JSX copy (`&apos;`/`&quot;`)
> and run `npm run build` (not just `tsc --noEmit`) before pushing this page.

**Live panel (top):**
- Big **state badge** (color-coded: OFF grey, ON_IDLE blue, READY amber,
  MOVING green, FAULT red) with "time in current state".
- **HV SoC** gauge (real `hv_soc`, %) and **LV battery voltage** readout (V,
  plus current/temp). No LV SoC.
- Quick chips: contactors, R2D, shutdown legs, active faults (reasons the
  classifier used — so it's explainable, not a black box).
- "Last transition" line.

**History panel (below):**
- A **timeline bar** of state segments (color blocks across time) for a chosen
  car + time range — driven by the standalone `car_status_segment` table, with
  **no drive-day dependency**.
- A table of segments (state, start, end, duration) with the **moving windows**
  highlighted and a total-moving summary.
- HV SoC + LV voltage trend over the range.
- A simple **time-range / "last 24h / 7d"** picker (not a drive-day picker).

Linked from the home splash grid (a new `SplashBox`, like trackside-live).

---

## 6. Config & thresholds (no magic numbers)

A single `car_status/config.py` (env-overridable), surfaced in the page's
"about/debug" area so they're transparent:

| Key | Meaning | Placeholder default | Needs team input |
|---|---|---|---|
| `HV_LIVE_V` | HV considered live above this | 50 V | confirm |
| `MOVE_RPM` | motor rpm = moving above | 50 | confirm |
| `MOVE_WHEEL` | mean wheel speed = moving above | 1.0 rad/s | confirm units |
| `MOVE_MPS` | gps speed = moving above | 0.8 m/s | confirm |
| `MIN_STATE_MS` | debounce before committing a state | 500 ms | tune |
| `MAX_GAP_MS` | tolerated dropout within a segment | 2000 ms | tune |

---

## 7. Open questions for the team

1. **Fault definition** — is "any nonzero `run_faults`" too aggressive? Should
   specific bits be whitelisted as non-faulting?
2. **Threshold values** — confirm `HV_LIVE_V`, `MOVE_RPM`, `MOVE_WHEEL` (and its
   units), `MOVE_MPS`, and the debounce timings (§6 are placeholders).
3. **Table location** — put `car_status_segment` in the Orion/Angelique DBs (per
   car) or a single shared DB? (Design assumes per-car alongside the existing
   telemetry tables, written by the processor with `CREATE TABLE IF NOT EXISTS`.)

*Out of scope (per direction):* `hvc_state_machine` (undocumented enum), LV SoC
estimate (no signal — LV shows voltage), and any drive-day / `classifier`
coupling (that infrastructure is abandoned).

---

## 8. Phased delivery plan

- **Phase 1 — classifier + live · IMPLEMENTED (this branch)**
  - `car_status/classifier.py` (pure, 20 unit tests passing) + processor
    `main.py` (decode → classify → emit `car_status` topic on transitions +
    heartbeat; hot-reloads `car_status_config`).
  - Viewer: `/api/car-status/stream` SSE bridge (shared bus) + `/api/car-status/
    config` POST (publishes to `car_status_config` via a shared KafkaJS producer).
  - `/car-status` page (client-only `ssr:false`): live state badge, HV SoC, LV
    voltage, reason chips, **live threshold sliders**. Home-page `SplashBox` link.
  - Registered in `server_devtool.sh` (`STACK_COMPONENTS` + `ALL_ORDER`); enable
    with `./server_devtool.sh enable car_status`. **No DB writes yet.**
- **Phase 2 — persistence + history · IMPLEMENTED (this branch)**
  - Standalone `car_status_segment` table added **schema-driven**: defined in
    `common_schema.sql`, ORM model in `analysis/sql_utils/models.py`, registered
    in `query_builder.py`; `sync_schema.sh Orion/Angelique` regenerated the init
    SQL + prisma. Standalone (car + time range), **no drive_day FK**.
  - Processor writes one row per closed segment via `db_writer.py` (lazy `get_db`
    + `QueryBuilder.insert`, same path as gps_classifier); avgs HV SoC / LV V and
    rolls up `active_faults` over the segment. Degrades to a no-op if the DB is
    unreachable, so live classification never stalls.
  - `/api/car-status/history` (by car + time range) returns segments +
    `movingWindows` + per-state totals. Reads via the **read-only pg pool**
    (`ReadOnlyDatabase`), NOT prisma — `prisma generate` is currently broken on
    `main` (pre-existing: `classifier`/`track_point` reference a missing `event`
    model), so raw pg avoids that dependency.
  - Page: history timeline bar + segment table + "moving" totals, with a
    1h/24h/7d range picker.
- **Phase 3 — CSV/MoTeC tagging · IMPLEMENTED (this branch)**
  - The MoTeC exporter writes a `*.status.json` sidecar per export segment
    (`exporter.ts` → `buildStatusSidecar`, fed by `telemetry.carStatusSegments`).
    It lists the OFF/IDLE/READY/MOVING windows overlapping the export, with
    elapsed-second offsets (`start_s`/`end_s`) + absolute ms + a `moving_s` total,
    so MoTeC/analysis tooling can jump straight to motion. Works for both `.ld`
    and `.csv` exports; best-effort (skipped if no segments / DB unavailable).
  - (Deferred) injecting markers into the `.ldx` lap format and a derived state
    channel column — the sidecar covers the core need without touching the rigid
    gate-based `.ldx` schema.
- **Phase 4 (optional)** — Grafana panel on `car_status_segment`, extra
  per-state analytics.

Each phase is independently shippable and reviewable.
