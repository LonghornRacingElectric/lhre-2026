# viewer_tool

Next.js viewer for live telemetry and replay workflows.

## Current multi-car status

Implemented:
- Header-driven live routing for `Orion` and `Angelique` (`car_type` respected end-to-end).
- Server-side car filtering in `/api/kafka-stream` via `?car=<orion|angelique>`.
- Canonical live topics from raw `sensor_data`: `car_visualization`, `driver_input_visualizer`, `map`, `live_banner`.
- Additional normalized live topics for remaining widgets: `dashboard_screen`, `timing_deltas`, `shutdown_screen`, `thermal_headroom`, `energy_budget`.
- Live car selector in `LiveViewerBanner` with URL/localStorage persistence and keyboard cycling (`[` / `]`).
- Rollout flag support with `NEXT_PUBLIC_VIEWER_MULTI_CAR` (set `false`/`0` to lock viewer to active/default car).
- Car-aware replay packet reads and handshake/end-event/status packet-end lookups.
- Thermal, energy, dashboard, shutdown, and timing widgets now consume live telemetry-backed contracts.

Still pending:
- Full sector/lap timing delta integration (current timing tile is telemetry pace-trend based).
- Add integration tests for simultaneous two-car streams and full widget parity.

## Kafka routing model

- Raw topics are consumed once by the backend Kafka consumer.
- Live widget topics are emitted on the in-process event bus.
- `kafka.routes.json` handles pass-through routing for derived topics (`gg-plot`, `track-mapper`).
- `sensor_data` normalization is handled directly in `src/lib/kafka/kafkaConsumer.ts`.

## Environment variables

Example viewer-side Kafka/env values:

```env
KAFKA_BROKERS=localhost:29092
KAFKA_CLIENT_ID=viewer-tool
KAFKA_GROUP_ID=viewer-tool-group
KAFKA_TOPICS=status,sensor_data,gg-plot,track-mapper
KAFKA_ROUTES_FILE=./kafka.routes.json
KAFKA_SENSOR_TOPIC=sensor_data
NEXT_PUBLIC_DEFAULT_LIVE_CAR=orion
NEXT_PUBLIC_VIEWER_MULTI_CAR=true
```

## Local setup

Install deps:

```bash
npm install
```

Generate Prisma clients:

```bash
npm run prisma-auth-generate
npm run prisma-telemtry-generate
npm run prisma-orion-generate
npm run prisma-angelique-generate
```

> Note: current Prisma schemas `prisma/{orion,telemtry}.prisma` include optional-list fields (`Float[]?`) that are rejected by Prisma 6.x. Resolve schema compatibility before relying on generated clients in fresh environments.

Generate protobuf TS stubs (optional helper scripts):

```bash
npm run protobuf-angelique
npm run protobuf-orion
```

Run dev server:

```bash
npm run dev
```
