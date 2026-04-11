#!/bin/bash
# Sync Schema Script
# Runs the SDD pipeline and updates the source tree with generated artifacts.

set -e

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
TELEMTRY_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
REPO_ROOT="$(cd "$TELEMTRY_ROOT/.." && pwd)"

CAR_NAME="${1:-Orion}"
CAR_LOWER="$(echo "$CAR_NAME" | tr '[:upper:]' '[:lower:]')"

SCRIPT_DIR="$TELEMTRY_ROOT/scripts"
INGEST_DIR="$TELEMTRY_ROOT/stack/ingest"
PRISMA_DIR="$TELEMTRY_ROOT/analysis/database/viewer_tool/prisma"
MODELS_FILE="$TELEMTRY_ROOT/analysis/sql_utils/models.py"

COMMON_SQL="$INGEST_DIR/common_schema.sql"
COMMON_PRISMA="$SCRIPT_DIR/common.prisma"

case "$CAR_NAME" in
  Orion)
    PROTO_FILE="$REPO_ROOT/drivers/longhorn-lib/protobuf/can_packets.proto"
    ROOT_MESSAGE="OrionSensorData"
    ;;
  Angelique)
    PROTO_FILE="$INGEST_DIR/protobuf/angelique.proto"
    ROOT_MESSAGE="AngeliqueSensorData"
    ;;
  *)
    PROTO_FILE="$REPO_ROOT/drivers/longhorn-lib/protobuf/can_packets.proto"
    ROOT_MESSAGE="auto"
    ;;
esac

GEN_DIR="$SCRIPT_DIR/gen_${CAR_LOWER}"
DB_INIT_FILE="$INGEST_DIR/${CAR_LOWER}_db_init.sql"
PRISMA_OUT="$PRISMA_DIR/${CAR_LOWER}.prisma"

if [ "$CAR_NAME" = "Orion" ] && [ "${REFRESH_COMMON_FROM_ORION:-0}" = "1" ]; then
  echo "Step 0: Refreshing common schema from Orion base schema..."
  awk '
    /^-- Generated Packet Table/ { skip=1; next }
    /^-- Track mapping tables/ { skip=0 }
    skip != 1 { print }
  ' "$INGEST_DIR/orion_db_init.sql" > "$COMMON_SQL"
fi

echo "Step 1: Generating common prisma..."
python3 "$SCRIPT_DIR/generate_schema.py" sql-to-prisma \
    "$COMMON_SQL" \
    "$COMMON_PRISMA"

echo "Step 2: Generating ${CAR_NAME} artifacts..."
python3 "$SCRIPT_DIR/generate_schema.py" proto-to-all \
    "$PROTO_FILE" \
    "$ROOT_MESSAGE" \
    "$GEN_DIR" \
    "$CAR_NAME"

echo "Step 3: Updating source tree..."

# Update SQL Init
cat "$COMMON_SQL" "$GEN_DIR/sensors.sql" > "$DB_INIT_FILE"

# Update Prisma
python3 "$SCRIPT_DIR/generate_schema.py" concat-prisma \
    "$COMMON_PRISMA" \
    "$GEN_DIR/sensors.prisma" \
    "$PRISMA_OUT" \
    "$CAR_NAME"

# Update SQLAlchemy Models
python3 "$SCRIPT_DIR/generate_schema.py" patch-models \
    "$MODELS_FILE" \
    "$GEN_DIR/models.py" \
    "$CAR_NAME"

echo "Step 4: Cleaning up..."
rm "$COMMON_PRISMA"
# We keep $GEN_DIR because it contains the generated 
# Python dataclasses used by processors for type safety.

echo "Done! ${CAR_NAME} SQL, Prisma, and ORM artifacts updated."
