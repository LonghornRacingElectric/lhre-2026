# Welcome message and options
Write-Host "Welcome to the Telemetry Server CLI Devtool. Please pick from the following methods to start the server:"
Write-Host "`t1) Start the server (ONLY)"
Write-Host "`t2) Delete the existing images"
Write-Host "`t3) Delete the existing images and telemetry_db volume"
Write-Host "`t4) Delete the existing images and both volumes (INCLUDING GRAFANA DASHBOARDS!)"
Write-Host "`tQ) Run Processor in background and start server"
Write-Host "`tW) Delete the existing server and processors images"
Write-Host ""

# Infinite loop to process user input
while ($true) {
    $opt = Read-Host "Choose an option (1-4, Q, W)"
    Write-Host ""
    Write-Host ""

    # Change directory to "ingest"
    Set-Location (Get-ChildItem -Recurse -Filter "ingest" | Select-Object -First 1).FullName
    if (-not (Test-Path .\ingest))
    {
        switch ($opt)
        {
            "1" {
                # Option 1: Start the server
                docker-compose down
                docker-compose up
                break
            }
            "2" {
                # Option 2: Delete the existing images
                docker-compose down
                $telemetryBackendImages = docker image ls --filter "reference=telemetry_backend" -q
                foreach ($image in $telemetryBackendImages)
                {
                    docker rmi $image
                }
                docker-compose up
                break
            }
            "3" {
                # Option 3: Delete the existing images and telemetry_db volume
                docker-compose down
                $telemetryBackendImages = docker image ls --filter "reference=telemetry_backend" -q
                foreach ($image in $telemetryBackendImages)
                {
                    docker rmi $image
                }
                docker volume rm telemetry_db
                docker volume create telemetry_db
                docker-compose up
                break
            }
            "4" {
                # Option 4: Delete the existing images and both volumes (INCLUDING GRAFANA DASHBOARDS!)
                while ($true)
                {
                    $yn = Read-Host "You are about to delete the Grafana dashboards saved on this computer. Are you sure you intend to delete this? [Y/n]"
                    Write-Host ""
                    Write-Host ""

                    if ($yn -match "^[Yy]$")
                    {
                        docker-compose down
                        $telemetryBackendImages = docker image ls --filter "reference=telemetry_backend" -q
                        foreach ($image in $telemetryBackendImages)
                        {
                            docker rmi $image
                        }
                        docker volume rm telemetry_db
                        docker volume create telemetry_db
                        docker volume rm grafana_storage
                        docker volume create grafana_storage
                        docker-compose up
                        break
                    }
                    elseif ($yn -match "^[Nn]$")
                    {
                        Write-Host "Crisis Averted!"
                        break
                    }
                    else
                    {
                        Write-Host "Invalid input, please try again."
                    }
                }
                break
            }
            "Q" {
                # Option Q: Run Processor in background and start server
                docker-compose down
                docker-compose up -d
                # Change directory to "processors"
                Set-Location (Get-ChildItem -Recurse -Directory | Where-Object { $_.Name -eq "processors" }).FullName
                if (-not (Test-Path .\processors))
                {
                    Write-Host "Failed to find processors"
                    exit
                }
                docker-compose down
                docker-compose up -d
                $processorContainerId = (docker container ls --filter "name=telemetry_processors" -q)
                Write-Host "Processor container ID: $processorContainerId"
                # Change directory back to "ingest"
                Set-Location (Get-ChildItem -Recurse -Directory | Where-Object { $_.Name -eq "ingest" }).FullName
                docker-compose logs -f
                break
            }
            "W" {
                # Option W: Delete the existing server and processors images
                docker-compose down
                $telemetryBackendImages = docker image ls --filter "reference=telemetry_backend" -q
                foreach ($image in $telemetryBackendImages)
                {
                    docker rmi $image
                }
                $telemetryProcessorsImages = docker image ls --filter "reference=telemetry_processors" -q
                foreach ($image in $telemetryProcessorsImages)
                {
                    docker rmi $image
                }
                docker-compose up -d
                # Change directory to "processors"
                Set-Location (Get-ChildItem -Recurse -Directory | Where-Object { $_.Name -eq "processors" }).FullName
                if (-not (Test-Path .\processors))
                {
                    Write-Host "Failed to find processors"
                    exit
                }
                docker-compose down
                docker-compose up -d
                $processorContainerId = (docker container ls --filter "name=telemetry_processors" -q)
                Write-Host "Processor container ID: $processorContainerId"
                # Change directory back to "ingest"
                Set-Location (Get-ChildItem -Recurse -Directory | Where-Object { $_.Name -eq "ingest" }).FullName
                docker-compose logs -f
                break
            }
            default {
                Write-Host "Invalid input, please try again."
            }
        }
    }
}
