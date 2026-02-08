#!/bin/bash
# Sync Schema Script
# Runs the SDD pipeline and updates the source tree with generated artifacts.

set -e

SCRIPT_DIR="scripts"
INGEST_DIR="stack/ingest"
PRISMA_DIR="analysis/database/viewer_tool/prisma"
MODELS_FILE="analysis/sql_utils/models.py"

echo "Step 1: Generating common prisma..."
python3 $SCRIPT_DIR/generate_schema.py sql-to-prisma \
    $INGEST_DIR/common_schema.sql \
    $SCRIPT_DIR/common.prisma

echo "Step 2: Generating Angelique artifacts..."
python3 $SCRIPT_DIR/generate_schema.py proto-to-all \
    $INGEST_DIR/protobuf/angelique.proto \
    AngeliqueSensorData \
    $SCRIPT_DIR/gen_angelique \
    Angelique

echo "Step 3: Updating source tree..."

# Update SQL Init
cat $INGEST_DIR/common_schema.sql $SCRIPT_DIR/gen_angelique/sensors.sql > $INGEST_DIR/angelique_db_init.sql

# Update Prisma
python3 $SCRIPT_DIR/generate_schema.py concat-prisma \
    $SCRIPT_DIR/common.prisma \
    $SCRIPT_DIR/gen_angelique/sensors.prisma \
    $PRISMA_DIR/angelique.prisma \
    Angelique

# Update SQLAlchemy Models
python3 $SCRIPT_DIR/generate_schema.py patch-models \
    $MODELS_FILE \
    $SCRIPT_DIR/gen_angelique/models.py \
    Angelique

echo "Step 4: Cleaning up..."
rm $SCRIPT_DIR/common.prisma
rm -rf $SCRIPT_DIR/gen_angelique

echo "Done! SQL, Prisma, and ORM artifacts updated."
