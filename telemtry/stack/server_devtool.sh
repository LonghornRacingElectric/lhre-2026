#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INGEST_DIR="$SCRIPT_DIR/ingest"
KAFKA_DIR="$SCRIPT_DIR/kafka"
PROCESSORS_DIR="$SCRIPT_DIR/processors"

cd_or_exit() {
    local dir="$1"
    local label="$2"
    cd "$dir" || (echo "Failed to find $label" && exit)
}

remove_images_by_repo() {
    local repo_pattern="$1"
    local image_ids

    image_ids="$($SUDO docker image ls --format '{{.Repository}} {{.ID}}' | awk -v p="$repo_pattern" '$1 ~ p {print $2}' | sort -u)"

    if [[ -n "$image_ids" ]]; then
        $SUDO docker rmi $image_ids
    else
        echo "No images found matching: $repo_pattern"
    fi
}

echo "Welcome to the Telemetry Server CLI Devtool. Please pick from the following methods to start the server:"
echo -e "\t1) Start the server (ONLY)"
echo -e "\t2) Delete the existing images"
echo -e "\t3) Delete the existing images and telemetry_db volume"
echo -e "\t4) Delete the existing images and both volumes (INCLUDING GRAFANA DASHBOARDS!)"
echo -e "\tQ) Run Processor in background and start server"
echo -e "\tW) Delete the existing server and processors images"
echo -e "\tE) Delete the lap timer processors images"
echo -e "\tF) Delete the gps classifier processors images"
echo


OS=$(uname)
if [[ "$OS" == "Linux" ]]; then
    SUDO="sudo"
else
    SUDO=""
fi


while :
do
    read -r opt
    opt="${opt//[[:space:]]/}"
    opt="${opt:0:1}"
    echo
    echo
    cd_or_exit "$INGEST_DIR" "ingest"
    if [[ -z "$opt" ]]; then
        echo "Invalid input, please try again."
        continue
    fi
    
    case $opt in
        1)
            $SUDO docker compose down
            $SUDO docker compose up
            break
            ;;
        2)
            cd_or_exit "$KAFKA_DIR" "kafka"
            $SUDO docker compose down
            $SUDO docker compose up -d
            cd_or_exit "$INGEST_DIR" "ingest"
            $SUDO docker compose down
            remove_images_by_repo "telemetry_backend"
            $SUDO docker compose up
            break
            ;;
        3)
            cd_or_exit "$KAFKA_DIR" "kafka"
            $SUDO docker compose down
            $SUDO docker compose up -d
            cd_or_exit "$INGEST_DIR" "ingest"
            $SUDO docker compose down
            remove_images_by_repo "telemetry_backend"
            $SUDO docker volume rm telemetry_db && $SUDO docker volume create telemetry_db
            $SUDO docker compose up
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
                        remove_images_by_repo "telemetry_backend"
                        $SUDO docker volume rm telemetry_db && $SUDO docker volume create telemetry_db
                        $SUDO docker volume rm grafana_storage && $SUDO docker volume create grafana_storage
                        $SUDO docker compose up
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
            $SUDO docker compose up -d
            cd_or_exit "$PROCESSORS_DIR" "processors"
            $SUDO docker compose down
            $SUDO docker compose up -d
            echo "Processor container ID: $($SUDO docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd_or_exit "$INGEST_DIR" "ingest"
            $SUDO docker compose logs -f
            break
            ;;
        w|W)
            $SUDO docker compose down
            remove_images_by_repo "telemetry_backend"
            remove_images_by_repo "telemetry_processors"
            $SUDO docker compose up -d
            cd_or_exit "$PROCESSORS_DIR" "processors"
            $SUDO docker compose down
            $SUDO docker compose up -d
            echo "Processor container ID: $($SUDO docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd_or_exit "$INGEST_DIR" "ingest"
            $SUDO docker compose logs -f
            break
            ;;
        e|E)
            cd_or_exit "$PROCESSORS_DIR/lap_timer" "processors"
            $SUDO docker compose down
            remove_images_by_repo "lap_timer"
            $SUDO docker compose up
            break
            ;;
        f|F)
            cd_or_exit "$PROCESSORS_DIR/gps_classifier" "processors"
            $SUDO docker compose down
            remove_images_by_repo "gps_classifier"
            $SUDO docker compose up
            break
            ;;
        g|G)
            cd_or_exit "$PROCESSORS_DIR/kafka_test" "processors"
            $SUDO docker compose down
            remove_images_by_repo "kafka_test"
            $SUDO docker compose up
            break
            ;;
        z|Z)
            $SUDO docker compose down
            remove_images_by_repo "telemetry_backend"
            $SUDO docker volume rm telemetry_db && $SUDO docker volume create telemetry_db
            $SUDO docker compose up -d
            cd_or_exit "$PROCESSORS_DIR/lap_timer" "processors"
            $SUDO docker compose down
            remove_images_by_repo "lap_timer"
            $SUDO docker compose up -d
            cd_or_exit "$PROCESSORS_DIR/gps_classifier" "processors"
            $SUDO docker compose down
            remove_images_by_repo "gps_classifier"
            $SUDO docker compose up
            # cd ../../ingest
            # $SUDO docker compose logs -f
            
            break
            ;;

        *)
            echo "Invalid input, please try again."
            ;;
    esac
done
