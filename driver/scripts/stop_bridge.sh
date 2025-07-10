#!/bin/bash
# ============================================================================
# I2C Bridge Stop Script
# ============================================================================

echo "Stopping I2C Bridge..."

# Kill bridge process
if [ -f /tmp/i2c_bridge.pid ]; then
    PID=$(cat /tmp/i2c_bridge.pid)
    echo "Killing bridge process (PID: ${PID})"
    kill ${PID} 2>/dev/null || true
    rm -f /tmp/i2c_bridge.pid
fi

# Kill any remaining bridge processes
pkill -f i2c_bridge 2>/dev/null || true

# Clean up pipes
echo "Cleaning up pipes..."
rm -f /tmp/i2c_sim_tx /tmp/i2c_sim_rx

echo "I2C Bridge stopped."
