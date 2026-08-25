# Running the stack on macOS (Docker)

The supported platform is still native Ubuntu 24.04 ([GETTING-STARTED](../GETTING-STARTED.md)).
This container is the workaround for Macs: Ubuntu 24.04 + ROS 2 Jazzy + Gazebo Harmonic
in Docker, with the desktop (RViz, Gazebo GUI) served to your browser via noVNC.
Runs at near-native speed on Apple Silicon (arm64 packages, software OpenGL for rendering).

## One-time setup

```bash
brew install colima docker docker-compose
# let docker find the compose plugin:
mkdir -p ~/.docker && cat > ~/.docker/config.json <<'EOF'
{ "cliPluginsExtraDirs": ["/opt/homebrew/lib/docker/cli-plugins"] }
EOF
colima start --cpu 6 --memory 8 --disk 60
cd autonomy/ros2/docker
docker compose up -d --build
```

## Daily use

- Desktop in browser: **http://localhost:6080/vnc.html?autoconnect=true&resize=scale**
- Shell into the container: `docker exec -it -u ubuntu lhr-autonomy bash`
- The repo is bind-mounted at `~/autonomy` inside the container — edit on the Mac
  with your normal editor, build/run inside the container:

```bash
cd ~/autonomy/ros2
./scripts/build.sh
./scripts/run_demo.sh                                  # kinematic demo
./scripts/run_gazebo_demo.sh track_style:=oval perception:=lidar
DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 ./scripts/rviz_demo.sh
```

(In a terminal opened inside the noVNC desktop, `DISPLAY` is already set.)

After a reboot: `colima start && docker start lhr-autonomy`.

## Notes

- `ros2/build`, `ros2/install`, `ros2/log` are created by the container and are
  Linux-only artifacts; delete them if you ever switch this checkout to native Ubuntu.
- Gazebo renders on CPU (llvmpipe) — expect ~0.7–0.8x real-time factor. Fine for
  development; use a native Ubuntu box for anything performance-sensitive.
- The image layers a scipy fix over `tiryoh/ros2-desktop-vnc:jazzy` (see Dockerfile).
