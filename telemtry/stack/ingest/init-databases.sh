#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="/app"

# Create databases if they don't exist (independent of POSTGRES_DB)
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -tAc \
  "SELECT 1 FROM pg_database WHERE datname='telemetry'" | grep -q 1 || \
  psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -c "CREATE DATABASE telemetry"

psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -tAc \
  "SELECT 1 FROM pg_database WHERE datname='angelique'" | grep -q 1 || \
  psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -c "CREATE DATABASE angelique"

psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -tAc \
  "SELECT 1 FROM pg_database WHERE datname='orion'" | grep -q 1 || \
  psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -c "CREATE DATABASE orion"

# Run DDL for each DB
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "telemetry"  -f "$SCRIPT_DIR/nightwatch_db_init.sql"
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "angelique"  -f "$SCRIPT_DIR/angelique_db_init.sql"
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "orion"  -f "$SCRIPT_DIR/orion_db_init.sql"