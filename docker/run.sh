#!/usr/bin/env bash
# Build or run the cFS + ESP32 bridge container.
#
#   ./docker/run.sh build            build the image
#   ./docker/run.sh sim              run against the simulated ESP32 (start tools/esp32_sim.py first)
#   ./docker/run.sh hw [PORT]        run against the real board (default PORT: /dev/ttyACM0)
#
# Stop a running container with Ctrl+C.
set -euo pipefail

IMAGE=cfs-esp32          # image names must be lowercase
NAME=cFS-ESP32           # container names can use capitals
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM_LINK=/tmp/ttyESP32

# Settings shared by both run modes (see compose.yaml for what each one does)
COMMON=(--rm --init --name "$NAME" --network host --sysctl fs.mqueue.msg_max=256)
if [ -t 0 ]; then COMMON+=(-it); else COMMON+=(-i); fi

# Only one container can have a given name at a time
if [ "${1:-}" = sim ] || [ "${1:-}" = hw ]; then
  if docker ps -a --format '{{.Names}}' | grep -qx "$NAME"; then
    echo "A container named $NAME already exists. Stop it first:" >&2
    echo "  docker stop $NAME    (or: docker rm -f $NAME)" >&2
    exit 1
  fi
fi

case "${1:-}" in
  build)
    docker build -f "$REPO_ROOT/docker/Dockerfile" -t "$IMAGE" "$REPO_ROOT"
    ;;

  sim)
    if [ ! -L "$SIM_LINK" ] || [ ! -e "$SIM_LINK" ]; then
      echo "Simulated ESP32 not found. Start it first in another terminal:" >&2
      echo "  python3 tools/esp32_sim.py" >&2
      exit 1
    fi
    # Docker needs the real /dev/pts/N path, not the symlink
    PTY="$(readlink -f "$SIM_LINK")"
    echo "Using simulated ESP32 on $PTY"
    exec docker run "${COMMON[@]}" \
      -v "$PTY":/dev/ttyUSB0 \
      --device-cgroup-rule='c 136:* rmw' \
      "$IMAGE"
    ;;

  hw)
    PORT="${2:-/dev/ttyACM0}"
    if [ ! -e "$PORT" ]; then
      echo "$PORT not found. Is the ESP32 plugged in? Available ports:" >&2
      ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null >&2 || echo "  (none)" >&2
      exit 1
    fi
    echo "Using ESP32 on $PORT"
    exec docker run "${COMMON[@]}" \
      --device "$PORT":/dev/ttyUSB0 \
      "$IMAGE"
    ;;

  *)
    sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
    ;;
esac
