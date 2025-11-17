# Telemetry Webtool

## Resetting the Database
- psql -h localhost -U electric -d angelique
- DELETE FROM dynamics; DELETE FROM controls; DELETE FROM pack; DELETE FROM diagnostics; DELETE FROM thermal; DELETE FROM packet;

## Normal Operating Procedures
### Starting the Docker Containers
- This starts kafka, the db, and the mqtt handler: `cd stack`, `./server_devtool.sh` and select option `2`
- To reset the database while starting the same services, run: `./server_devtool.sh` and select option `3` instead.
- To start a processor, look into option e,f,z.
- To start on windows platforms, use `.\win_server_devtools.ps1` instead (same option logic applies). 

### Starting the webtool
- Make sure that the `.env` file is fully complete
- Move to the Next.js root `cd analysis/database/viewer_tool`
- To load the database schemas for prisma, run: `npm run prisma-auth-generate` (loading auth database), `npm run prisma-angelique-generate` (loading angelique database) and `npm run prisma-telemtry-generate` (loading nightwatch database)
- To launch the website in dev mode, run `npm run dev`
- To launch the website for deployment, run `npm run build`

### Maintaining the Webtool
- To add a new user account, run `npm run prisma-auth-seed -- <username> <password>`

## First Time Setup Procedures
# Setup Telemetry Server Locally

1. Download Docker.
    1. If working with server on Linux, paste the following commands:

    ```bash
    sudo apt update && sudo apt -y upgrade
    sudo apt-get install ca-certificates curl gnupg
    sudo install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    sudo chmod a+r /etc/apt/keyrings/docker.gpg
    echo \
      "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
      "$(. /etc/os-release && echo "$VERSION_CODENAME")" stable" | \
      sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    sudo apt-get update
    sudo apt-get -y install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
    ```

    1. If working on Windows, download Docker Desktop here : https://docs.docker.com/get-started/introduction/get-docker-desktop/
    2. For MacOS users, follow setup here: https://docs.docker.com/desktop/setup/install/mac-install/#install-and-run-docker-desktop-on-mac
2. Download Python
    1. For specific version of python, the docker stack uses Python 3.11.4. For simplicity and to avoid potential weird behavior across Docker and non-containerized development, we recommend downloading Python 3.11.x. Find Python downloads here: https://www.python.org/downloads/
3. Git clone the Telemetry Repo: https://github.com/LonghornRacingElectric/telemtry
    1. This is the telemetry repo. Please checkout out a new branch. Don’t work on main branch.
    2. Move to monorepo coming soon
4. Download required Python modules
    1. Create a python virtual environment and activate the virtual environment. Good python knowledge to know.  Please use a virtual environment, you don’t want conflicting software versions.
        1. run `python -m venv venv`
        2. To activate the virtual environment, if using linux/MacOS run `source venv/bin/activate`, if using windows run `venv/Scripts/activate`
    2. Use analysis/requirements.txt file.
        1. pip install -r requirements.txt
5. Setup the Docker Server
    1. If you clone straight from the main branch, you need make these changes in the code for local development
        1. In stack/ingest/docker-compose.yml file, find this snippet.

            ```yaml
              GF_SERVER_ROOT_URL: 'https://lhrelectric.org/grafana/'
              GF_SERVER_SERVE_FROM_SUB_PATH: 'true'
            ```

        2. Change that snippet to this:

            ```yaml
              GF_SERVER_ROOT_URL: 'http://localhost:3000'
              GF_SERVER_SERVE_FROM_SUB_PATH: 'false'
            ```

    2. Navigate to stack/ingest, then run `docker compose up`

6. There will be Errors, more setup is needed here
    1. If you have an external network error, run `docker network create telemetry_network`
    2. If volumes don’t exist, run `docker volume create grafana_storage` and `docker volume create telemetry_db`
7. Run `docker compose up` and if server starts with a bunch of logging messages, you have finished setting up the telemetry server.
    1. Look for this in terminal: `YOU ARE IN DEBUGING MODE` followed by some warnings and `Connection Pool Fully Conected!`

8. Every time you wish to stop the server, press control C to stop the server, then run `docker compose down`. Please follow this step when stopping the running server

