#!/bin/bash
# ============================================================================
# I2C Bridge Startup Script
# ============================================================================
# This script starts the I2C bridge for software driver testing

set -e

# Configuration
I2C_PIPE_BASE="/tmp/i2c_sim"
BRIDGE_BINARY="./build/i2c_bridge"

echo "Starting I2C Bridge..."
echo "======================="

# Check if bridge binary exists
if [ ! -f ${BRIDGE_BINARY} ]; then
    echo "Bridge binary not found. Building..."
    make all
fi

# Clean up any existing pipes
echo "Cleaning up existing pipes..."
rm -f ${I2C_PIPE_BASE}_tx ${I2C_PIPE_BASE}_rx

echo "Starting I2C bridge..."
${BRIDGE_BINARY} &

BRIDGE_PID=$!
echo "I2C bridge started with PID: ${BRIDGE_PID}"

# Save PID for cleanup
echo ${BRIDGE_PID} > /tmp/i2c_bridge.pid

echo ""
echo "I2C Bridge is ready!"
echo "==================="
echo "Bridge PID: ${BRIDGE_PID}"
echo "Pipe base path: ${I2C_PIPE_BASE}"
echo ""
echo "You can now run I2C driver tests:"
echo "  ./build/test_i2c_driver"
echo "  ./build/interactive_test"
echo ""
echo "To stop the bridge:"
echo "  ./scripts/stop_bridge.sh"
echo ""
echo "Press Ctrl+C to stop the bridge"

# Wait for interrupt
trap "echo 'Stopping bridge...'; kill ${BRIDGE_PID} 2>/dev/null || true; rm -f /tmp/i2c_bridge.pid; exit 0" INT

wait ${BRIDGE_PID}
