# SDD System Usage Guide

The Schema Driven Development (SDD) system automates the synchronization of database schemas, ORM models, and frontend Prisma definitions. 

For high-level architectural details and motivation, see [SCHEMA_DRIVEN_DEV.md](./SCHEMA_DRIVEN_DEV.md).

## 1. The Workflow (The "LHR Way")

When adding a sensor or modifying the car configuration:
1.  **Define:** Modify the `.proto` file in `stack/ingest/protobuf/`.
2.  **Generate:** Run the sync script to update the source tree.
3.  **Implement:** Use the generated Dataclasses and SQLAlchemy models in your processors or frontend.

### Automatic Synchronization
The fastest way to update your local environment is to use the sync script:
```bash
bash scripts/sync_schema.sh
```
This script runs the generator for Angelique and automatically updates:
- `stack/ingest/{car}_db_init.sql` (Database)
- `analysis/database/viewer_tool/prisma/{car}.prisma` (Frontend)
- `analysis/sql_utils/models.py` (ORM)

When run for `Orion` (the default), it also syncs:
- `analysis/database/viewer_tool/protobuf/orion.proto` from `drivers/longhorn-lib/protobuf/can_packets.proto`.

---

## 2. The Role of Generated Dataclasses (`dataclasses.py`)

While Protobuf provides the wire format, the generated `dataclasses.py` provides a **Pythonic, type-safe interface** for internal logic. 

### Why not just use Protobuf objects directly?
1.  **IDE Support:** Standard Python dataclasses provide superior autocompletion and hover-documentation in VSCode/PyCharm compared to compiled Protobuf objects.
2.  **Immutability & Logic:** Dataclasses are easier to extend with helper methods or convert into other formats (like JSON) using standard Python libraries.
3.  **Type Hinting:** They allow for strict type hinting across your processors, making bugs easier to catch before runtime.

### Location
The dataclasses are generated into car-specific folders:
`telemtry/scripts/gen_{car}/dataclasses.py`

### Key Components
- **`{Car}SensorData`**: The root container for a full telemetry packet.
- **Sensor Modules**: Individual classes (e.g., `AngeliqueDynamics`, `AngeliqueBattery`) containing the actual sensor fields.

---

## 3. Usage in Processors

Processors interact with telemetry data in three stages: **Decoding**, **Logic**, and **Persistence**.

### Stage 1: Decoding
Always decode raw bytes using the standard Protobuf library.
```python
from stack.ingest.protobuf import angelique_pb2
from google.protobuf.json_format import MessageToDict

msg = angelique_pb2.AngeliqueSensorData()
msg.ParseFromString(payload)
# Convert to dict for easier handling with ORM/Dataclasses
data_dict = MessageToDict(msg, preserving_proto_field_name=True)
```

### Stage 2: Logic (Dataclasses)
Use generated Dataclasses for complex calculations. They provide IDE autocompletion and type safety.
```python
from scripts.gen_angelique.dataclasses import AngeliqueSensorData

# Instantiate from the decoded dictionary
data = AngeliqueSensorData(**data_dict)

if data.dynamics.vcu_velocity:
    speed = sum(v**2 for v in data.dynamics.vcu_velocity)**0.5
```

### Stage 3: Persistence (ORM)
Use SQLAlchemy models in `analysis/sql_utils/models.py` to save data. 

**Important:** SQLAlchemy constructors expect keyword arguments. If you want to use a Dataclass instance for persistence, you must unpack it using `asdict()` or pass attributes manually.

The recommended way to save a packet and its sensors is to use **relationships**:

```python
from dataclasses import asdict
from analysis.sql_utils.db_session import get_db
from analysis.sql_utils.models import AngeliquePacket, AngeliqueDynamics

with get_db("Angelique") as session:
    # Create the root packet
    packet = AngeliquePacket(packet_id=data_dict['packet_id'], time=data_dict['time'])
    
    # Option A: Use the decoded dictionary (Efficient)
    packet.dynamics = AngeliqueDynamics(**data_dict['dynamics'])
    
    # Option B: Use the Dataclass (if you modified data in Stage 2)
    # data = AngeliqueSensorData(**data_dict)
    # packet.dynamics = AngeliqueDynamics(**asdict(data.dynamics))
    
    session.add(packet)
    session.commit()
```

### Comprehensive Example
The following code demonstrates a typical processor loop utilizing all generated artifacts:

```python
import angelique_pb2
from dataclasses import asdict
from google.protobuf.json_format import MessageToDict
from scripts.gen_angelique.dataclasses import AngeliqueSensorData
from analysis.sql_utils.models import AngeliquePacket, AngeliqueDynamics
from analysis.sql_utils.db_session import get_db

def on_message(payload_bytes):
    # 1. DECODE raw bytes
    msg = angelique_pb2.AngeliqueSensorData()
    msg.ParseFromString(payload_bytes)
    data_dict = MessageToDict(msg, preserving_proto_field_name=True)

    # 2. LOGIC using Dataclass (Type Stubs)
    typed_data = AngeliqueSensorData(**data_dict)
    
    if typed_data.dynamics and typed_data.dynamics.vcu_accel:
        accel_mag = sum(a**2 for a in typed_data.dynamics.vcu_accel)**0.5
        # ... logic ...

    # 3. PERSISTENCE using ORM Relationships
    with get_db("Angelique") as session:
        # Create packet and attach dynamics
        packet = AngeliquePacket(packet_id=typed_data.packet_id, time=typed_data.time)
        
        if typed_data.dynamics:
            # Unpack the dataclass into the ORM model
            packet.dynamics = AngeliqueDynamics(**asdict(typed_data.dynamics))
            
        session.add(packet)
        session.commit()
```

---

## 3. Modifying Infrastructure
To add lookup tables (LUTs), metadata fields (like new event settings), or SQL functions:
1.  Modify `telemtry/stack/ingest/common_schema.sql`.
2.  Run `bash scripts/sync_schema.sh`.
3.  The changes will propagate to the Prisma schema used by the Viewer Tool.

---

## 4. Manual Pipeline Control
If you need to run specific parts of the generator manually:

```bash
# Generate shared infrastructure Prisma
python3 scripts/generate_schema.py sql-to-prisma \
    stack/ingest/common_schema.sql \
    scripts/common.prisma

# Generate sensor-specific artifacts
python3 scripts/generate_schema.py proto-to-all \
    stack/ingest/protobuf/angelique.proto \
    AngeliqueSensorData \
    scripts/gen_angelique \
    Angelique

# Concatenate Prisma models
python3 scripts/generate_schema.py concat-prisma \
    scripts/common.prisma \
    scripts/gen_angelique/sensors.prisma \
    analysis/database/viewer_tool/prisma/angelique.prisma \
    Angelique

# Patch ORM models (Uses BEGIN/END markers in models.py)
python3 scripts/generate_schema.py patch-models \
    analysis/sql_utils/models.py \
    scripts/gen_angelique/models.py \
    Angelique
```

---

## 5. Key Files
- `telemtry/scripts/generate_schema.py`: The transformation engine.
- `telemtry/scripts/sync_schema.sh`: Convenience script for local developers.
- `drivers/longhorn-lib/protobuf/can_packets.proto`: Orion source-of-truth protobuf.
- `telemtry/stack/ingest/common_schema.sql`: Source of truth for static infrastructure.
- `telemtry/analysis/sql_utils/models.py`: SQLAlchemy models (Shared + Car-specific).
- `telemtry/scripts/gen_angelique/dataclasses.py`: Generated Python type stubs.

---

## 6. Live-Viewer Validation (Schema-First)

When validating live-viewer widgets, publish test packets from protobuf schema (not ORM reflection):

```bash
source telemtry/.venv/bin/activate
python telemtry/analysis/database/paho_testing.py --car Orion --profile viewer --schema-source proto
```

`paho_testing.py` now supports `--schema-source`:
- `proto` (default): uses compiled protobuf descriptors as the source of truth (SDD-aligned).
- `orm`: uses SQLAlchemy/query-builder table specs (legacy behavior).

For end-to-end validation (MQTT -> Kafka -> viewer topics), use:

```bash
source telemtry/.venv/bin/activate
python telemtry/analysis/database/validate_live_viewer.py --car Both --rows 20 --delay 0.03
```

This checks:
- `sensor_data` includes expected protobuf table payloads.
- Live widget topics (`live_banner`, `dashboard_screen`, `driver_input_visualizer`, `car_visualization`,
  `thermal_headroom`, `energy_budget`, `map`) receive fresh, changing values.
- Optional ORM drift reporting against protobuf schema (`--strict-orm-sync` to fail on drift).

Processor-backed topics are separate from the core ingest path:
- `gg-plot` requires `telemtry/stack/processors/gg_plot`.
- `track-mapper`/lap timing require `telemtry/stack/processors/track_mapper` and `telemtry/stack/processors/lap_timer`.

`server_devtool.sh` option `3` only starts Kafka + ingest, so `gg-plot` and `track-mapper` will stay empty unless those processors are started.
