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


OS=$(uname)
if [[ "$OS" == "Linux" ]]; then
    SUDO="sudo"
else
    SUDO=""
fi


while :
do
    read -n 1 opt
    echo
    echo
    cd $(find . -name "ingest") || (echo "Failed to find ingest" && exit)
    id "postgres" > /dev/null 2>&1 && $SUDO pkill -u postgres
    case $opt in
        1)
            $SUDO docker compose down
            $SUDO docker compose up
            break
            ;;
        2)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_backend | awk '{print $3}')"
            $SUDO docker compose up
            break
            ;;
        3)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_backend | awk '{print $3}')"
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
                        $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_backend | awk '{print $3}')"
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
            cd ../processors || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up -d
            echo "Processor container ID: $($SUDO docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd ../ingest
            $SUDO docker compose logs -f
            break
            ;;
        w|W)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_backend | awk '{print $3}')"
            $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_processors | awk '{print $3}')"
            $SUDO docker compose up -d
            cd ../processors || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker compose up -d
            echo "Processor container ID: $($SUDO docker container ls | grep telemetry_processors | awk '{print $1}')"
            cd ../ingest
            $SUDO docker compose logs -f
            break
            ;;
        e|E)
            cd ../processors/lap_timer || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_processors | awk '{print $3}')"
            $SUDO docker compose up
            break
            ;;
        f|F)
            cd ../processors/gps_classifier || (echo "Failed to find processors" && exit)
            $SUDO docker compose down
            $SUDO docker rmi "$($SUDO docker image ls | grep telemetry_processors | awk '{print $3}')"
            $SUDO docker compose up
            break
            ;;
        *)
            echo "Invalid input, please try again."
            ;;
    esac
done
