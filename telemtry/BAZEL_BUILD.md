# Telemetry System - Bazel Build Guide

This document describes the Bazel build setup for the telemetry system, including Docker container builds, testing, and deployment.

## Architecture Overview

The telemetry system consists of the following components on the `telemetry_network` Docker network:

### Infrastructure Services (External Images)
- **Mosquitto** (MQTT Broker): Receives telemetry data from the car
- **PostgreSQL**: Stores processed telemetry data (Nightwatch & Angelique databases)
- **Kafka**: Message queue for processor communication
- **Grafana**: Visualization dashboard

### Custom Services (Built with Bazel)
- **Ingest Service** (`//telemtry/stack/ingest`): 
  - Subscribes to MQTT topics
  - Decodes protobuf/pickle/base64 payloads
  - Writes to PostgreSQL
  - Forwards to Kafka `sensor_data` topic

- **GPS Classifier** (`//telemtry/stack/processors/gps_classifier`):
  - Consumes from MQTT
  - Classifies driving patterns (turns, acceleration)
  - Writes classifications to PostgreSQL

- **Lap Timer** (`//telemtry/stack/processors/lap_timer`):
  - Tracks lap times using GPS gate detection
  - Writes lap data to PostgreSQL

- **Kafka Base** (`//telemtry/stack/processors/kafka_base`):
  - Base Kafka consumer template
  - Demonstrates Kafka → processing flow

### Non-Docker Components
- **Analysis Library** (`//telemtry/analysis`):
  - Database utilities (`sql_utils/`)
  - Data visualization tools
  - Testing utilities (`paho_testing.py`)

## Quick Start

### Build All Docker Images
```bash
bazel build //telemtry:telemetry_images
```

### Build and Load Images to Local Docker
```bash
# Build tarball and load to Docker
bazel run //telemtry/stack/ingest:ingest_tarball
bazel run //telemtry/stack/processors/gps_classifier:gps_classifier_tarball
bazel run //telemtry/stack/processors/lap_timer:lap_timer_tarball
bazel run //telemtry/stack/processors/kafka_base:kafka_base_tarball
```

### Run Unit Tests (No Docker Required)
```bash
bazel test //telemtry:unit_tests
```

### Run Integration Tests (Requires Docker Containers Running)
```bash
# First, start the Docker stack
cd telemtry/stack/ingest && docker-compose up -d
cd telemtry/stack/kafka && docker-compose up -d

# Then run integration tests
bazel test //telemtry:integration_tests --test_tag_filters=integration
```

### Run Full Integration Test with Docker Lifecycle
```bash
bazel test //telemtry:full_integration_tests --test_tag_filters=manual
```

## Target Reference

### Build Targets

| Target | Description |
|--------|-------------|
| `//telemtry:telemetry_images` | All Docker images |
| `//telemtry:telemetry_tarballs` | All Docker tarballs for local loading |
| `//telemtry:telemetry_all` | Everything (Docker + non-Docker) |
| `//telemtry:telemetry_lib` | All Python libraries |
| `//telemtry:config_files` | Configuration files |
| `//telemtry:compose_files` | All docker-compose files |

### Individual Service Targets

| Service | Image Target | Tarball Target | Push Target |
|---------|--------------|----------------|-------------|
| Ingest | `//telemtry/stack/ingest:ingest_image` | `:ingest_tarball` | `:ingest_push` |
| GPS Classifier | `//telemtry/stack/processors/gps_classifier:gps_classifier_image` | `:gps_classifier_tarball` | `:gps_classifier_push` |
| Lap Timer | `//telemtry/stack/processors/lap_timer:lap_timer_image` | `:lap_timer_tarball` | `:lap_timer_push` |
| Kafka Base | `//telemtry/stack/processors/kafka_base:kafka_base_image` | `:kafka_base_tarball` | `:kafka_base_push` |

### Test Targets

| Target | Description | Docker Required |
|--------|-------------|-----------------|
| `//telemtry:unit_tests` | Unit tests (protobuf, analysis) | No |
| `//telemtry:integration_tests` | Connectivity & data flow tests | Yes |
| `//telemtry:full_integration_tests` | Full stack with Docker lifecycle | Yes (managed) |
| `//telemtry:telemetry_tests` | All tests | Yes |

## Data Flow Testing

The integration tests validate the following data flows:

1. **MQTT → Ingest → Database**
   - Protobuf serialized sensor data
   - Pickle serialized data (legacy)
   - Base64 encoded data (Angelique)

2. **MQTT → Ingest → Kafka**
   - `sensor_data` topic forwarding

3. **Kafka → Processor**
   - Consumer group message processing
   - Protobuf decoding in consumers

4. **Config Flow**
   - `config/flask` event start/stop
   - `config/test` processor configuration

## Adding New Tests

Tests are located in `telemtry/stack/tests/`. To add a new test:

1. Create a new Python test file (e.g., `test_my_feature.py`)
2. Add the test target to `telemtry/stack/tests/BUILD.bazel`:
   ```python
   py_test(
       name = "my_feature_test",
       srcs = ["test_my_feature.py"],
       deps = [
           ":test_utils",
           # Add dependencies
       ],
       tags = ["integration"],  # or ["unit"] for non-Docker tests
   )
   ```
3. Add to the appropriate test suite in `telemtry/BUILD.bazel`

## Adding New Processors

To add a new Kafka consumer processor:

1. Start from template: copy `telemtry/stack/processors/kafka_base/` to `telemtry/stack/processors/my_processor/`
2. Update Python files and Docker metadata for your processor
3. Create `BUILD.bazel` following the pattern in `gps_classifier/BUILD.bazel`
4. Add to the aggregate targets in:
   - `telemtry/stack/processors/BUILD.bazel`
   - `telemtry/stack/BUILD.bazel`
   - `telemtry/BUILD.bazel`

Template guidance and processor best practices are documented in:
`telemtry/stack/processors/kafka_base/README.md`

## Environment Configuration

The telemetry system uses environment variables for configuration:

- `IN_DOCKER`: Set to `1` when running in Docker container
- `POSTGRES_USER`, `POSTGRES_PASSWORD`: Database credentials
- `SERVER_TARGET`: Target server (LOCAL, SUBNET, EXTERNAL)
- `LOGLEVEL`: Logging level (DEBUG, INFO, WARNING, ERROR)

See `.env.example` for all available options.

## CI/CD Integration

For CI/CD pipelines:

```bash
# Build all images
bazel build //telemtry:telemetry_images

# Run unit tests (fast, no Docker)
bazel test //telemtry:unit_tests

# Push images to registry
bazel run //telemtry/stack/ingest:ingest_push
bazel run //telemtry/stack/processors/gps_classifier:gps_classifier_push
# etc.
```

## Troubleshooting

### "No module named 'paho.mqtt'"
Run `bazel sync` to ensure dependencies are downloaded.

### Docker network issues
Ensure the `telemetry_network` exists:
```bash
docker network create telemetry_network
```

### Container not starting
Check logs:
```bash
docker logs ingest
docker logs kafka
```
