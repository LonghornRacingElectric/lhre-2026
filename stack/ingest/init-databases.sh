#!/bin/bash
set -e

# Initialize the main telemetry database
# The default POSTGRES_DB is 'telemetry', which is created by the entrypoint
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "telemetry" -f /app/nightwatch_db_init.sql

# Create and initialize the test database
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "postgres" -c "CREATE DATABASE angelique"
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "angelique" -f /app/angelique_db_init.sql
