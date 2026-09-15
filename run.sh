#!/bin/bash
# ============================================================
# cFS STM32 Telemetry — Run Script
# Starts cFS, STM32 serial bridge, and sensor GUI
# ============================================================

CFS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="$CFS_DIR/tools"
CFS_EXE="$CFS_DIR/build-native_std/exe/cpu1"

# ── Colours ───────────────────────────────────────────────
RED='\033[0;31m'
GRN='\033[0;32m'
YLW='\033[0;33m'
BLU='\033[0;34m'
NC='\033[0m'

echo -e "${BLU}=================================================="
echo -e " cFS STM32 Telemetry — Run"
echo -e "==================================================${NC}"

# ── Check cFS binary ──────────────────────────────────────
if [ ! -f "$CFS_EXE/core-cpu1" ]; then
    echo -e "${RED}ERROR: core-cpu1 not found."
    echo -e "Run './build.sh' first.${NC}"
    exit 1
fi

# ── Check STM32 serial port ───────────────────────────────
SERIAL_PORT=""
for port in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyUSB0 /dev/ttyUSB1; do
    if [ -e "$port" ]; then
        SERIAL_PORT="$port"
        break
    fi
done

if [ -z "$SERIAL_PORT" ]; then
    echo -e "${YLW}WARNING: No STM32 serial port found."
    echo -e "Bridge will retry automatically when device connects.${NC}"
else
    echo -e "${GRN}Found STM32 on: $SERIAL_PORT${NC}"
fi

# ── Check Python scripts ──────────────────────────────────
BRIDGE_SCRIPT="$TOOLS_DIR/STM32Bridge.py"
GUI_SCRIPT="$TOOLS_DIR/sensor_gui.py"

if [ ! -f "$BRIDGE_SCRIPT" ]; then
    echo -e "${RED}ERROR: Bridge script not found at $BRIDGE_SCRIPT${NC}"
    exit 1
fi

if [ ! -f "$GUI_SCRIPT" ]; then
    echo -e "${RED}ERROR: GUI script not found at $GUI_SCRIPT${NC}"
    exit 1
fi

# ── Cleanup function ──────────────────────────────────────
cleanup() {
    echo ""
    echo -e "${YLW}Shutting down...${NC}"
    kill $CFS_PID   2>/dev/null
    kill $BRIDGE_PID 2>/dev/null
    kill $GUI_PID   2>/dev/null
    wait 2>/dev/null
    echo -e "${GRN}Done.${NC}"
}
trap cleanup EXIT INT TERM

# ── Start cFS ─────────────────────────────────────────────
echo ""
echo -e "${BLU}[1/3] Starting cFS...${NC}"
cd "$CFS_EXE"
./core-cpu1 &
CFS_PID=$!
echo -e "  PID: $CFS_PID"

# Wait for cFS to reach operational state
echo "  Waiting for cFS to initialize..."
sleep 4

if ! kill -0 $CFS_PID 2>/dev/null; then
    echo -e "${RED}ERROR: cFS failed to start${NC}"
    exit 1
fi
echo -e "${GRN}  cFS running${NC}"

# ── Start STM32 bridge ────────────────────────────────────
echo ""
echo -e "${BLU}[2/3] Starting STM32 bridge...${NC}"
cd "$CFS_DIR"
python3 "$BRIDGE_SCRIPT" &
BRIDGE_PID=$!
echo -e "  PID: $BRIDGE_PID"
sleep 2

if ! kill -0 $BRIDGE_PID 2>/dev/null; then
    echo -e "${YLW}WARNING: Bridge exited early (STM32 may not be connected)${NC}"
else
    echo -e "${GRN}  Bridge running${NC}"
fi


# ── Start GUI ─────────────────────────────────────────────
echo ""
echo -e "${BLU}[3/3] Starting sensor GUI...${NC}"
python3 "$GUI_SCRIPT" &
GUI_PID=$!
echo -e "  PID: $GUI_PID"
echo ""
echo -e "${GRN}=================================================="
echo -e " All systems running"
echo -e " Press Ctrl+C to stop everything"
echo -e "==================================================${NC}"
echo ""
echo -e "  cFS:    PID $CFS_PID"
echo -e "  Bridge: PID $BRIDGE_PID"
echo -e "  GUI:    PID $GUI_PID"
echo ""

# Wait for GUI to close
wait $GUI_PID
