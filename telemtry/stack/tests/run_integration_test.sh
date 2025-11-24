#!/bin/bash

# telemtry/stack/tests/run_integration_test.sh

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration ---
# Adjust these paths to your actual docker-compose files.
# The paths are relative to the script's location in the Bazel runfiles directory.
# You can find the exact paths by running the test with --test_output=streamed
# and adding 'ls -R' to the script.
INGEST_COMPOSE_FILE="../ingest/docker-compose.yml"
KAFKA_COMPOSE_FILE="../kafka/docker-compose.yml"
# Add other compose files as needed

# The test client script
TEST_CLIENT_SCRIPT="./stack_test_client"


# --- Teardown Function ---
# This function will be called to clean up resources.
cleanup() {
  echo "--- Tearing down Docker containers ---"
  docker-compose -f "${KAFKA_COMPOSE_FILE}" -f "${INGEST_COMPOSE_FILE}" down --volumes
}

# Register the cleanup function to be called on script exit.
trap cleanup EXIT


# --- Main Test Logic ---

echo "--- Starting Docker containers in the background ---"
# Use docker-compose to bring up all services.
# The '-d' flag runs them in detached mode.
docker-compose -f "${KAFKA_COMPOSE_FILE}" -f "${INGEST_COMPOSE_FILE}" up --build -d

echo "--- Waiting for services to become healthy ---"
# In a real-world scenario, you would use a more robust health check.
# This could be a script that polls service endpoints until they are ready.
# For this example, we'll just wait for a fixed amount of time.
sleep 30

echo "--- Running test client ---"
# Execute the Python test client.
# The client script should exit with a non-zero status if tests fail.
"${TEST_CLIENT_SCRIPT}"

echo "--- Tests passed ---"

# The 'trap' command will automatically call the cleanup function upon exit.
exit 0
