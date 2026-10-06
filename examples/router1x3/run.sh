#!/bin/bash
set -e

# Detect Yosys executable
YOSYS_CMD="yosys"
if [ -x "/eda/yosys/bin/yosys" ]; then
    YOSYS_CMD="/eda/yosys/bin/yosys"
fi

echo "Starting Router1x3 synthesis and schematic export..."
$YOSYS_CMD -m vajra synth.ys
echo "Schematics exported successfully to images/ directory."
