# Kafka Processor Starter Template (`kafka_base`)

Use this folder as the baseline for new Kafka processors that consume `sensor_data` protobuf packets.

## What this template includes

- Kafka consumer loop with manual offset commits
- `car_type` header handling (`Angelique`/`Orion`/`Nightwatch`)
- Direct protobuf decode (no dependency on ingest runtime modules)
- Docker + docker-compose scaffolding

## Best practices for new processors

1. Decode protobuf directly from `stack.ingest.protobuf.*_pb2` in your processor.
2. Keep a dedicated consumer group ID per processor (`KAFKA_GROUP_ID`).
3. Commit offsets only after your batch processing logic succeeds.
4. Preserve `car_type` metadata when publishing derived topics.
5. Keep Docker images minimal: copy only processor code + protobuf definitions unless extra dependencies are required.
6. Configure `logging.basicConfig(...)` and emit a periodic "waiting for messages" info log so `docker compose up` is observably healthy.

## Quick start for a new processor

1. Copy this directory to `telemtry/stack/processors/<your_processor>`.
2. Rename image/container names in `docker-compose.yml`.
3. Update `main.py`:
   - input/output topic names
   - per-message processing logic
   - emitted payload schema
4. Build and run:

```bash
cd telemtry/stack/processors/<your_processor>
docker compose up -d --build
docker logs -f <your_container_name>
```

## Environment variables

- `KAFKA_BOOTSTRAP_SERVERS` (default: `kafka:9092`)
- `KAFKA_INPUT_TOPIC` (default: `sensor_data`)
- `KAFKA_GROUP_ID` (default in template: `kafka-base-group`)
- `KAFKA_DEFAULT_CAR` (default: `Angelique`)
- `LOGLEVEL` (default: `INFO`)
