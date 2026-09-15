#!/bin/bash
# ============================================================
# cFS STM32 Telemetry — Build Script
# Builds cFS and installs to build-native_std/exe/cpu1
# ============================================================

set -e

CFS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=================================================="
echo " cFS STM32 Telemetry — Build"
echo " Directory: $CFS_DIR"
echo "=================================================="

# ── Check dependencies ────────────────────────────────────
echo ""
echo "[1/4] Checking dependencies..."

check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "  MISSING: $1 — install with: $2"
        MISSING=1
    else
        echo "  OK: $1"
    fi
}

MISSING=0
check_dep cmake    "sudo apt-get install cmake"
check_dep gcc      "sudo apt-get install gcc"
check_dep g++      "sudo apt-get install g++"
check_dep make     "sudo apt-get install make"
check_dep python3  "sudo apt-get install python3"
check_dep pip3     "sudo apt-get install python3-pip"

if [ "$MISSING" -eq 1 ]; then
    echo ""
    echo "ERROR: Missing dependencies. Install them and re-run."
    exit 1
fi

# ── Check Python packages ─────────────────────────────────
echo ""
echo "[2/4] Checking Python packages..."

check_py() {
    if ! python3 -c "import $1" &>/dev/null; then
        echo "  Installing: $1"
        pip3 install "$2" --break-system-packages 2>/dev/null || \
        pip3 install "$2"
    else
        echo "  OK: $1"
    fi
}

check_py serial    pyserial
check_py matplotlib matplotlib
check_py tkinter   tk

# ── Build cFS ─────────────────────────────────────────────
echo ""
echo "[3/4] Building cFS (native_std)..."
cd "$CFS_DIR"

if [ ! -f "build-native_std/stamp.prep" ]; then
    echo "  Running cmake prep..."
    make native_std.prep
fi

echo "  Running make install..."
make native_std.install

# ── Verify output ─────────────────────────────────────────
echo ""
echo "[4/4] Verifying build output..."

if [ -f "build-native_std/exe/cpu1/core-cpu1" ]; then
    echo "  OK: core-cpu1 binary found"
else
    echo "  ERROR: core-cpu1 not found — build may have failed"
    exit 1
fi

if [ -f "build-native_std/exe/cpu1/cf/cfe_es_startup.scr" ]; then
    echo "  OK: startup script found"
else
    echo "  ERROR: startup script not found"
    exit 1
fi

echo ""
echo "=================================================="
echo " Build complete!"
echo " Run './run.sh' to start cFS + bridge + GUI"
echo "=================================================="
