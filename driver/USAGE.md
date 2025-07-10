# I2C Driver Test Suite - Quick Start Guide

## Overview
This I2C driver test suite provides a complete software testing environment for I2C functionality using named pipes for communication between the driver and a simulated hardware backend.

## What's Built
- **i2c_bridge**: Background service that simulates I2C hardware
- **simple_test**: Basic functionality test
- **interactive_test**: Command-line interface for manual testing
- **test_i2c_driver**: Comprehensive automated test suite

## Quick Start

### 1. Start the I2C Bridge (Terminal 1)
```bash
cd /Users/makarand/i2c/driver
make start-bridge
```

This will:
- Start the I2C bridge service
- Create named pipes for communication
- Simulate I2C slave devices (addresses 0x50, 0x51, 0x52)
- Display the bridge PID for later cleanup

### 2. Run Tests (Terminal 2)
```bash
cd /Users/makarand/i2c/driver

# Option A: Run simple test
make simple

# Option B: Run interactive test
make interactive

# Option C: Run comprehensive test suite
make test
```

### 3. Stop the Bridge
```bash
make stop-bridge
```

## Available Tests

### Simple Test (`make simple`)
- Basic driver initialization
- Device open/close
- Write operation to I2C slave
- Read operation from I2C slave
- Device scanning

### Interactive Test (`make interactive`)
Interactive shell with commands:
- `init` - Initialize driver
- `open` - Open I2C device
- `write 0x50 0x01 0x02` - Write data to address
- `read 0x50 4` - Read 4 bytes from address
- `scan` - Scan for I2C devices
- `debug on/off` - Toggle debug output
- `quit` - Exit

### Comprehensive Test (`make test`)
- All basic operations
- Error condition testing
- Performance measurements
- Large data transfers
- Multi-message transactions

## Simulated I2C Devices

The bridge creates these virtual I2C devices:
- **0x50**: 256-byte EEPROM-like device
- **0x51**: 128-byte sensor-like device
- **0x52**: 64-byte small device

## Example Usage Session

```bash
# Terminal 1: Start bridge
$ make start-bridge
Starting I2C Bridge...
=======================
I2C bridge started with PID: 12345
Bridge ready for commands

# Terminal 2: Run tests
$ make simple
Simple I2C Driver Test
======================
1. Initializing I2C driver...
Driver initialized successfully

2. Opening I2C device...
Device opened successfully

3. Testing basic write operation...
Write successful: 4 bytes written

4. Testing basic read operation...
Read successful: 4 bytes read
Data: 0x00 0x01 0x02 0x03

# When done
$ make stop-bridge
```

## Architecture

```
┌─────────────────┐    Named Pipes     ┌─────────────────┐
│   Test Apps     │ ◄─────────────────► │   I2C Bridge    │
│                 │ /tmp/i2c_sim_tx     │                 │
│ • simple_test   │ /tmp/i2c_sim_rx     │ • Pipe Handler  │
│ • interactive   │                     │ • Slave Sim     │
│ • test_suite    │                     │ • Memory Model  │
└─────────────────┘                     └─────────────────┘
```

## Troubleshooting

### "Failed to open I2C device: I/O error"
- Make sure the bridge is running: `make start-bridge`
- Check pipes exist: `ls -la /tmp/i2c_sim*`

### Bridge fails to start
- Kill any existing bridges: `make stop-bridge`
- Check port availability: `lsof -i :5000` (if using TCP)
- Check file permissions in `/tmp/`

### Tests timeout
- Bridge may be overloaded
- Restart bridge: `make stop-bridge && make start-bridge`
- Check system resources with `top`

## Files Created
- `/tmp/i2c_sim_tx` - Command pipe to bridge
- `/tmp/i2c_sim_rx` - Response pipe from bridge
- `/tmp/i2c_bridge.pid` - Bridge process ID

## Next Steps
- Modify `test/simple_test.c` for custom tests
- Add new I2C devices in `i2c_bridge.c`
- Integrate with real hardware by replacing pipe communication
- Add more comprehensive protocol testing
