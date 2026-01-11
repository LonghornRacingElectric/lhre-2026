#!/bin/bash

# telemtry/stack/tests/run_integration_test.sh
#
# Integration test runner for the telemetry stack.
# This script manages the Docker container lifecycle and runs integration tests.
#
# Usage:
#   bazel test //telemtry/stack/tests:stack_integration_test
#   # or directly:
#   ./run_integration_test.sh

set -e

# --- Configuration ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TELEMTRY_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Docker compose file paths (relative to telemtry directory)
INGEST_COMPOSE="$TELEMTRY_DIR/stack/ingest/docker-compose.yml"
KAFKA_COMPOSE="$TELEMTRY_DIR/stack/kafka/docker-compose.yml"
GPS_COMPOSE="$TELEMTRY_DIR/stack/processors/gps_classifier/docker-compose.yml"
LAP_TIMER_COMPOSE="$TELEMTRY_DIR/stack/processors/lap_timer/docker-compose.yml"
KAFKA_BASE_COMPOSE="$TELEMTRY_DIR/stack/processors/kafka_base/docker-compose.yml"

# Test client
TEST_CLIENT="$SCRIPT_DIR/stack_test_client"

# Network name
NETWORK_NAME="telemetry_network"

# --- Color Output ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# --- Cleanup Function ---
cleanup() {
    log_info "Tearing down Docker containers..."
    
    # Stop containers in reverse order
    docker-compose -f "$GPS_COMPOSE" down --volumes 2>/dev/null || true
    docker-compose -f "$LAP_TIMER_COMPOSE" down --volumes 2>/dev/null || true
    docker-compose -f "$KAFKA_BASE_COMPOSE" down --volumes 2>/dev/null || true
    docker-compose -f "$INGEST_COMPOSE" down --volumes 2>/dev/null || true
    docker-compose -f "$KAFKA_COMPOSE" down --volumes 2>/dev/null || true
    
    log_info "Cleanup complete"
}

# Register cleanup on exit
trap cleanup EXIT

# --- Create Docker Network ---
create_network() {
    log_info "Creating Docker network: $NETWORK_NAME"
    docker network create "$NETWORK_NAME" 2>/dev/null || {
        log_warn "Network $NETWORK_NAME already exists"
    }
}

# --- Create Docker Volumes ---
create_volumes() {
    log_info "Creating Docker volumes..."
    docker volume create grafana_storage 2>/dev/null || true
    docker volume create telemetry_db 2>/dev/null || true
}

# --- Wait for Service ---
wait_for_service() {
    local service_name="$1"
    local host="$2"
    local port="$3"
    local timeout="${4:-60}"
    
    log_info "Waiting for $service_name at $host:$port..."
    
    local start_time=$(date +%s)
    while true; do
        if nc -z "$host" "$port" 2>/dev/null; then
            log_info "$service_name is ready"
            return 0
        fi
        
        local elapsed=$(($(date +%s) - start_time))
        if [ "$elapsed" -ge "$timeout" ]; then
            log_error "Timeout waiting for $service_name"
            return 1
        fi
        
        sleep 2
    done
}

# --- Start Infrastructure Services ---
start_infrastructure() {
    log_info "Starting infrastructure services (Kafka, Ingest)..."
    
    # Start Kafka first
    docker-compose -f "$KAFKA_COMPOSE" up --build -d
    wait_for_service "Kafka" "localhost" "9092" 120
    
    # Start Ingest (includes Mosquitto, PostgreSQL, Grafana)
    docker-compose -f "$INGEST_COMPOSE" up --build -d
    wait_for_service "Mosquitto" "localhost" "1883" 60
    wait_for_service "PostgreSQL" "localhost" "5432" 60
    wait_for_service "Grafana" "localhost" "3000" 60
    
    log_info "Infrastructure services are running"
}

# --- Start Processor Services ---
start_processors() {
    log_info "Starting processor services..."
    
    # Start processors (they depend on Kafka and MQTT)
    docker-compose -f "$GPS_COMPOSE" up --build -d || log_warn "GPS classifier failed to start"
    docker-compose -f "$LAP_TIMER_COMPOSE" up --build -d || log_warn "Lap timer failed to start"
    docker-compose -f "$KAFKA_BASE_COMPOSE" up --build -d || log_warn "Kafka base failed to start"
    
    # Give processors time to initialize
    sleep 10
    
    log_info "Processor services are running"
}

# --- Run Tests ---
run_tests() {
    log_info "Running integration tests..."
    
    # Check if test client exists
    if [ -x "$TEST_CLIENT" ]; then
        "$TEST_CLIENT"
        return $?
    fi
    
    # Fall back to running Python tests directly
    log_info "Running Python tests..."
    
    cd "$SCRIPT_DIR"
    
    # Run connectivity tests first
    python3 -m pytest test_mqtt_connectivity.py -v || return 1
    python3 -m pytest test_kafka_connectivity.py -v || return 1
    python3 -m pytest test_db_connectivity.py -v || return 1
    
    # Run data flow tests
    python3 -m pytest test_data_flow.py -v || return 1
    
    return 0
}

# --- Main ---
main() {
    log_info "=== Telemetry Stack Integration Test ==="
    log_info "Starting test at $(date)"
    
    # Create network and volumes
    create_network
    create_volumes
    
    # Start services
    start_infrastructure
    start_processors
    
    # Run tests
    if run_tests; then
        log_info "=== All tests passed ==="
        exit 0
    else
        log_error "=== Tests failed ==="
        exit 1
    fi
}

main "$@"
