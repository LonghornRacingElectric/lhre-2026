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
echo

while :
do
    read -n 1 opt
    echo
    echo
    cd $(find . -name "ingest") || (echo "Failed to find ingest" && exit)
    brew services stop postgresql
    case $opt in
        1)
            docker compose down
            docker compose up
            break
            ;;
        2)
            docker compose down
            docker rmi "$(docker image ls | grep telemetry_backend | awk '{print $3}')"
            docker compose up
            break
            ;;
        3)
            docker compose down
            docker rmi "$(docker image ls | grep telemetry_backend | awk '{print $3}')"
            docker volume rm telemetry_db && docker volume create telemetry_db
            docker compose up
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
                        docker compose down
                        docker rmi "$(docker image ls | grep telemetry_backend | awk '{print $3}')"
                        docker volume rm telemetry_db && docker volume create telemetry_db
                        docker volume rm grafana_storage && docker volume create grafana_storage
                        docker compose up
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
            docker compose down
            docker compose up -d
            cd ../processors || (echo "Failed to find processors" && exit)
            docker compose down
            docker compose up -d
            echo "Processor container ID: $(docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd ../ingest
            docker compose logs -f
            break
            ;;
        w|W)
            docker compose down
            docker rmi "$( docker image ls | grep telemetry_backend | awk '{print $3}')"
            docker rmi "$( docker image ls | grep telemetry_processors | awk '{print $3}')"
            docker compose up -d
            cd ../processors || (echo "Failed to find processors" && exit)
            docker compose down
            docker compose up -d
            echo "Processor container ID: $(docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd ../ingest
            docker compose logs -f
            break
            ;;
        e|E)
            cd ../processors/lap_timer || (echo "Failed to find processors" && exit)
            docker compose down
            docker rmi "$(docker image ls | grep lap_timer | awk '{print $3}')"
            docker compose up
            break
            ;;
        f|F)
            cd ../processors/gps_classifier || (echo "Failed to find processors" && exit)
            docker compose down
            docker rmi "$(docker image ls | grep gps_classifier | awk '{print $3}')"
            docker compose up
            break
            ;;
        *)
            echo "Invalid input, please try again."
            ;;
    esac
done