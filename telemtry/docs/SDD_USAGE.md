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
This script runs the generator for Angelique and updates `angelique_db_init.sql` and `angelique.prisma`.

---

## 2. Usage in Processors

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
Use SQLAlchemy models in `analysis/sql_utils/models.py` to save data. The field names are guaranteed to match your decoded dictionary.
```python
from analysis.sql_utils.db_session import get_db
from analysis.sql_utils.models import AngeliqueDynamics

with get_db("Angelique") as session:
    # Use unpacking to map dict fields to the Model
    row = AngeliqueDynamics(**data_dict['dynamics'])
    session.add(row)
    session.commit()
```

### Comprehensive Example
The following code demonstrates a typical processor loop utilizing all generated artifacts:

```python
import angelique_pb2
from google.protobuf.json_format import MessageToDict
from scripts.gen_angelique.dataclasses import AngeliqueSensorData
from analysis.sql_utils.models import AngeliqueDynamics
from analysis.sql_utils.db_session import get_db

def on_message(payload_bytes):
    # 1. DECODE raw bytes
    msg = angelique_pb2.AngeliqueSensorData()
    msg.ParseFromString(payload_bytes)
    data_dict = MessageToDict(msg, preserving_proto_field_name=True)

    # 2. LOGIC using Dataclass (Type Stubs)
    # This provides full IDE autocompletion for sensor fields
    typed_data = AngeliqueSensorData(**data_dict)
    
    if typed_data.dynamics and typed_data.dynamics.vcu_accel:
        # Perform typed calculations
        accel_mag = sum(a**2 for a in typed_data.dynamics.vcu_accel)**0.5
        print(f"Current Accel: {accel_mag:.2f} m/s^2")

    # 3. PERSISTENCE using ORM
    # The dictionary keys from Stage 1 match the Model columns exactly
    with get_db("Angelique") as session:
        db_row = AngeliqueDynamics(**data_dict['dynamics'])
        session.add(db_row)
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
```

---

## 5. Bazel Integration
Bazel is used for CI and hermetic builds. It generates artifacts in `bazel-out/` but does not modify your source tree.
```bash
bazel build //telemtry/scripts:gen_angelique_artifacts
```

## 6. Key Files
- `telemtry/scripts/generate_schema.py`: The transformation engine.
- `telemtry/scripts/sync_schema.sh`: Convenience script for local developers.
- `telemtry/stack/ingest/common_schema.sql`: Source of truth for static infrastructure.
- `telemtry/analysis/sql_utils/models.py`: SQLAlchemy models (Shared + Car-specific).
- `telemtry/scripts/gen_angelique/dataclasses.py`: Generated Python type stubs.
