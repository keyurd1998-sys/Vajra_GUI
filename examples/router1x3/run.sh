#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# If built locally, make local build directory available to Yosys
if [ -f "$REPO_DIR/build/vajra.so" ]; then
    export YOSYS_PLUGIN_PATH="$REPO_DIR/build:${YOSYS_PLUGIN_PATH}"
fi

# Detect Yosys executable (custom $YOSYS env var, PATH, or local fallback)
YOSYS_CMD="${YOSYS:-$(command -v yosys 2>/dev/null || true)}"
if [ -z "$YOSYS_CMD" ] && [ -x "/eda/yosys/bin/yosys" ]; then
    YOSYS_CMD="/eda/yosys/bin/yosys"
fi

if [ -z "$YOSYS_CMD" ]; then
    echo "Error: Yosys executable not found in PATH or at /eda/yosys/bin/yosys."
    echo "Please ensure Yosys is installed or set the YOSYS environment variable."
    exit 1
fi

echo "Using Yosys: $YOSYS_CMD"
echo "Starting Router1x3 synthesis and schematic export..."
"$YOSYS_CMD" -m vajra "$SCRIPT_DIR/synth.ys"
echo "Schematics exported successfully to images/ directory."
