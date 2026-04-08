# Multi-car live viewer plan (Orion + Angelique)

## Goal
Run Orion and Angelique simultaneously through telemetry processors and the viewer, with a reliable UI toggle to select which car is shown live across widgets.

## Current implementation status

### Completed in this session
1. **Processor hardening**
   - `gg_plot`, `kafka_base`, and `track_mapper` now decode by Kafka `car_type` header (fallback car supported).
   - Processor consumer groups are now unique/env-configurable (no shared `test-group` collision).
   - `gg_plot` and `track_mapper` now publish with `car_type` headers.
2. **Viewer stream backend**
    - `sensor_data` decoding now supports Angelique + Orion payloads.
    - Canonical live widget routes are emitted from backend for:
      - `car_visualization`
      - `driver_input_visualizer`
      - `map`
      - `live_banner`
      - `dashboard_screen`
      - `timing_deltas`
      - `shutdown_screen`
      - `thermal_headroom`
      - `energy_budget`
    - `/api/kafka-stream` now supports server-side `car` filtering and subscribes raw source topics for requested logical topics.
    - `useKafkaStream`/`useKafkaJSON` stale handling now respects caller-provided values.
3. **Live car toggle UI**
    - Added global car-selection provider/context.
    - `LiveViewerBanner` now includes live car selector (`Orion` / `Angelique`), plus URL + localStorage persistence.
    - Keyboard quick-switch supported with `[` and `]`.
    - Added `NEXT_PUBLIC_VIEWER_MULTI_CAR` feature flag for staged rollout (single-car lock when disabled).
    - Live widgets (`GGPlot`, `Map`, `CarVisualization`, `DriverInputVisualizer`, `TrackMapper`, `LiveViewerBanner`) now apply selected-car filtering.
4. **Widget parity progress**
   - Replaced placeholder-only widgets with live telemetry contracts:
     - `DashboardScreen` now renders live vehicle and thermal metrics.
     - `ShutdownScreen` now renders shutdown/contactors diagnostics when present.
     - Thermal/Energy tiles now render live telemetry-backed metrics.
     - `TimingDeltas` now renders telemetry pace trend (rolling speed delta).
5. **API/Prisma car-aware behavior**
   - Added shared car-prisma resolver helper.
   - Updated `handshake`, `set-event-status`, and `end-event` packet lookups to use car-aware packet DBs.
   - Replay endpoints now read packet timeline data from selected event car.
   - Kept existing telemetry prisma binding and layered explicit car-based packet-db selection where packet reads occur.
6. **Docs and config**
   - Added/updated viewer docs in `analysis/database/viewer_tool/README.md`.
   - Updated viewer routing config (`kafka.routes.json`) to consistent array-based rule format.
   - Added Orion protobuf schema file for viewer runtime decode support (`protobuf/orion.proto`).

## Open work (next steps)

1. **Widget parity / realism**
   - Replace telemetry pace trend in `TimingDeltas` with true sector/lap delta calculations.
   - Broaden shutdown cross-car mapping so all cars expose equivalent leg-level semantics.
2. **Integration coverage**
   - Add explicit two-car integration tests for:
     - concurrent sensor_data ingest
     - header preservation through processors
     - selected-car-only rendering across live widgets
3. **Environment hardening**
   - Resolve Prisma schema compatibility issues in `prisma/{telemtry,orion}.prisma` (optional list syntax) and make client generation reliable in CI/dev scripts.
   - Ensure protobuf generation toolchain (`protoc`) is installed in dev/CI images where needed.

## Reference files touched by the rollout
- Processors:
  - `telemtry/stack/processors/gg_plot/main.py`
  - `telemtry/stack/processors/kafka_base/main.py`
  - `telemtry/stack/processors/track_mapper/main.py`
- Viewer backend:
  - `telemtry/analysis/database/viewer_tool/src/lib/kafka/kafkaConsumer.ts`
  - `telemtry/analysis/database/viewer_tool/src/app/api/kafka-stream/route.ts`
  - `telemtry/analysis/database/viewer_tool/kafka.routes.json`
  - `telemtry/analysis/database/viewer_tool/src/hooks/useKafkaStream.ts`
- Viewer UI/state:
  - `telemtry/analysis/database/viewer_tool/src/lib/car.ts`
  - `telemtry/analysis/database/viewer_tool/src/lib/carSelection.tsx`
  - `telemtry/analysis/database/viewer_tool/src/components/LiveViewerBanner.tsx`
  - `telemtry/analysis/database/viewer_tool/src/components/{GGPlot,Map,CarVisualization,DriverInputVisualizer,TrackMapper}.tsx`
  - `telemtry/analysis/database/viewer_tool/src/components/Providers.tsx`
- API/prisma:
  - `telemtry/analysis/database/viewer_tool/src/lib/prisma/{telemtry,carPrisma}.ts`
  - `telemtry/analysis/database/viewer_tool/src/app/api/{handshake,end-event,set-event-status}/route.ts`
  - `telemtry/analysis/database/viewer_tool/src/app/api/replay/{stream,summary}/route.ts`
