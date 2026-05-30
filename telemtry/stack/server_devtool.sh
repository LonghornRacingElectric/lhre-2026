#!/bin/bash

echo "Welcome to the Telemetry Server CLI Devtool. Please pick from the following methods to start the server:"
echo -e "\t1) Start the server (ONLY)"
echo -e "\t2) Delete the existing images"
echo -e "\t3) Delete the existing images and telemetry_db volume"
echo -e "\t4) Delete the existing images and both volumes (INCLUDING GRAFANA DASHBOARDS!)"
echo -e "\tQ) Run Processor in background and start server"
echo -e "\tW) Delete the existing server and processors images"
echo -e "\tE) Delete the lap timer processors images"
echo -e "\tF) Delete the gps classifier processors images"
echo -e "\tH) Delete the track mapper processors images"
echo -e "\tI) Start field enricher processor (derived real-time fields)"
echo -e "\tS) Stop everything currently running"
echo


OS=$(uname)
if [[ "$OS" == "Linux" ]]; then
    SUDO="sudo"
else
    SUDO=""
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INGEST_DIR="$SCRIPT_DIR/ingest"
DASHBOARD_SOURCE_DIR="$SCRIPT_DIR/../analysis/database/dashboards"

remove_images_matching() {
    local pattern="$1"
    local ids
    ids=$($SUDO docker image ls | grep "$pattern" | awk '{print $3}')
    if [[ -n "$ids" ]]; then
        # shellcheck disable=SC2086
        $SUDO docker rmi $ids
    fi
}

compose_down_if_present() {
    local compose_dir="$1"
    local label="$2"

    if [[ ! -f "$compose_dir/docker-compose.yml" ]]; then
        return
    fi

    (
        cd "$compose_dir" || exit 1
        echo "Stopping $label..."
        if ! $SUDO docker compose down; then
            echo "Failed to stop $label."
        fi
    )
}

ensure_grafana_dashboards_exist() {
    if [[ ! -d "$DASHBOARD_SOURCE_DIR" ]]; then
        echo "Grafana dashboard source directory not found: $DASHBOARD_SOURCE_DIR"
        return 1
    fi

    echo "Grafana dashboards found and ready for auto-provisioning."
}

stop_everything_running() {
    compose_down_if_present "$SCRIPT_DIR/ingest" "ingest services"
    compose_down_if_present "$SCRIPT_DIR/kafka" "kafka services"
    compose_down_if_present "$SCRIPT_DIR/processors" "processor services"
    compose_down_if_present "$SCRIPT_DIR/processors/lap_timer" "lap timer processor"
    compose_down_if_present "$SCRIPT_DIR/processors/gps_classifier" "gps classifier processor"
    compose_down_if_present "$SCRIPT_DIR/processors/track_mapper" "track mapper processor"
    compose_down_if_present "$SCRIPT_DIR/processors/kafka_test" "kafka test processor"
    compose_down_if_present "$SCRIPT_DIR/processors/field_enricher" "field enricher processor"

    echo "Stopped all telemetry stack services."
}


while :
do
    read -n 1 opt
    echo
    echo
    cd "$INGEST_DIR" || (echo "Failed to find ingest" && exit)
    if [[ "$OS" == "Linux" ]]; then
        id "postgres" > /dev/null 2>&1 && $SUDO pkill -u postgres
    else
        brew services stop postgresql
    fi

    case $opt in
        1|2|3|4|q|Q|w|W|z|Z)
            ensure_grafana_dashboards_exist || exit 1
            ;;
    esac
    
    case $opt in
        1)
            $SUDO docker compose down
            $SUDO docker compose up
            break
            ;;
        2)
            cd ../kafka || (echo "Failed to find kafka" && exit)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep kafka-bridge | awk '{print $3}')"
            $SUDO docker compose up --build -d
            cd ../processors/field_enricher || (echo "Failed to find field_enricher" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            cd ../../ingest || (echo "Failed to find ingest" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build
            break
            ;;
        3)
            cd ../kafka || (echo "Failed to find kafka" && exit)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep kafka-bridge | awk '{print $3}')"
            $SUDO docker compose up --build -d
            cd ../processors/field_enricher || (echo "Failed to find field_enricher" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            cd ../../ingest || (echo "Failed to find ingest" && exit)
            $SUDO docker compose down
            $SUDO docker volume rm telemetry_db 2>/dev/null; $SUDO docker volume create telemetry_db
            $SUDO docker compose up --build
            break
            ;;
        4)
            while :
            do
                echo "You are about to delete the Grafana dashboards saves on this computer. Are you sure you intend to delete this? [Y/n]"
                read -n 1 yn
                echo
                echo
                case $yn in
                    Y|y)
                        $SUDO docker compose down
                        $SUDO docker volume rm telemetry_db 2>/dev/null; $SUDO docker volume create telemetry_db
                        $SUDO docker volume rm grafana_storage 2>/dev/null; $SUDO docker volume create grafana_storage
                        $SUDO docker compose up --build
                        break
                        ;;
                    N|n)
                        echo "Crisis Averted!"
                        break
                        ;;
                    *)
                        echo "Invalid input, please try again."
                        ;;
                esac
            done
            break
            ;;
        q|Q)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            cd ../processors || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            echo "Processor container ID: $($SUDO docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd ../ingest
            $SUDO docker compose logs -f
            break
            ;;
        w|W)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            cd ../processors || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            echo "Processor container ID: $($SUDO docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd ../ingest
            $SUDO docker compose logs -f
            break
            ;;
        e|E)
            cd ../processors/lap_timer || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build
            break
            ;;
        f|F)
            cd ../processors/gps_classifier || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build
            break
            ;;
        g|G)
            cd ../processors/kafka_test || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build
            break
            ;;
        h|H)
            cd ../processors/track_mapper || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build
            break
            ;;
        i|I)
            cd ../processors/field_enricher || (echo "Failed to find field_enricher" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            echo "Field enricher container ID: $($SUDO docker container ls | grep field_enricher_processor | awk '{print $1}')"
            break
            ;;
        s|S)
            stop_everything_running
            break
            ;;
        z|Z)
            $SUDO docker compose down
            $SUDO docker volume rm telemetry_db 2>/dev/null; $SUDO docker volume create telemetry_db
            $SUDO docker compose up --build -d
            cd ../processors/lap_timer || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build -d
            cd ../gps_classifier || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up --build
            # cd ../../ingest
            # $SUDO docker compose logs -f
            
            break
            ;;

        *)
            echo "Invalid input, please try again."
            ;;
    esac
done
