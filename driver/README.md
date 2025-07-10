# I2C Driver Test Suite

A comprehensive software driver test suite for the I2C master controller RTL design. This suite provides a Linux-compatible device driver interface that communicates with the RTL simulation through named pipes on macOS.

## Overview

The I2C driver test suite consists of several components:

- **I2C Driver Library** (`i2c_driver.c/.h`) - Core driver implementing Linux-compatible I2C API
- **RTL Bridge** (`i2c_bridge.c`) - Bridge between driver and RTL simulation using named pipes  
- **Test Suite** (`test_i2c_driver.c`) - Automated comprehensive tests
- **Interactive Tool** (`interactive_test.c`) - Command-line interface for manual testing

## Architecture

```
┌─────────────────┐    Named Pipes    ┌─────────────────┐    Signals    ┌─────────────────┐
│   Test Apps     │ ◄────────────────► │   I2C Bridge    │ ◄────────────► │  RTL Simulation │
│                 │                    │                 │                │                 │
│ • Test Suite    │                    │ • Pipe Mgmt     │                │ • I2C Master    │
│ • Interactive   │                    │ • Slave Sim     │                │ • Testbench     │
│ • Driver API    │                    │ • Protocol      │                │ • VCD Output    │
└─────────────────┘                    └─────────────────┘                └─────────────────┘
```

## Features

### Driver API Features
- Linux-compatible I2C device driver interface
- Support for multiple I2C message transfers
- Write, read, and combined write-read operations
- Configurable timeouts and error handling
- Debug logging and transaction tracing
- Device reset and status monitoring

### Test Suite Features
- Comprehensive automated testing
- Error condition testing (NACK, timeouts)
- Performance measurements
- Large data transfer testing
- Multi-message transaction testing
- Interactive command-line interface

### Bridge Features
- Named pipe communication on macOS
- Simulated I2C slave devices with memory
- Protocol translation between driver and RTL
- Background operation with signal handling
- Automatic RTL simulation management

## Building

### Prerequisites
- GCC compiler with C99 support
- Make build system
- POSIX-compatible system (macOS/Linux)
- Icarus Verilog (for RTL simulation)

### Build Commands

```bash
# Build all components
make all

# Build individual components
make driver/build/libi2c_driver.a    # Driver library
make driver/build/i2c_bridge         # RTL bridge
make driver/build/test_i2c_driver    # Test suite
make driver/build/interactive_test   # Interactive tool

# Clean build artifacts
make clean
```

## Usage

### Quick Start

1. **Build everything:**
   ```bash
   make all
   ```

2. **Run complete test cycle:**
   ```bash
   make full-test
   ```

3. **Manual testing:**
   ```bash
   # Terminal 1: Start bridge
   make start-bridge
   
   # Terminal 2: Run interactive test
   make interactive
   ```

### Manual Process

1. **Start the RTL simulation and bridge:**
   ```bash
   make start-bridge
   ```

2. **Run automated tests:**
   ```bash
   make test
   ```

3. **Or use interactive testing:**
   ```bash
   make interactive
   ```

4. **Stop the bridge when done:**
   ```bash
   make stop-bridge
   ```

### Interactive Commands

The interactive test tool provides a shell-like interface:

```
i2c> help                           # Show available commands
i2c> init                           # Initialize driver
i2c> open                           # Open I2C device
i2c> write 0x50 0x01 0x02 0x03      # Write data to address 0x50
i2c> read 0x50 4                    # Read 4 bytes from address 0x50
i2c> scan                           # Scan for I2C devices
i2c> status                         # Show device status
i2c> debug on                       # Enable debug output
i2c> quit                           # Exit
```

## API Reference

### Core Functions

```c
// Initialize I2C driver
int i2c_init(i2c_device_t *device, const i2c_config_t *config);

// Open/close device
int i2c_open(i2c_device_t *device);
int i2c_close(i2c_device_t *device);

// Basic I/O operations
int i2c_read(i2c_device_t *device, uint16_t addr, uint8_t *buf, size_t len);
int i2c_write(i2c_device_t *device, uint16_t addr, const uint8_t *buf, size_t len);

// Advanced operations
int i2c_transfer(i2c_device_t *device, i2c_msg_t *msgs, int num_msgs);
int i2c_write_read(i2c_device_t *device, uint16_t addr, 
                   const uint8_t *write_buf, size_t write_len,
                   uint8_t *read_buf, size_t read_len);

// Device management
int i2c_reset(i2c_device_t *device);
uint32_t i2c_get_status(i2c_device_t *device);
void i2c_set_timeout(i2c_device_t *device, uint32_t timeout_ms);
```

### Example Usage

```c
#include "driver/include/i2c_driver.h"

int main() {
    i2c_device_t device;
    i2c_config_t config = {
        .clock_freq = 50000000,
        .i2c_freq = 100000,
        .addr_width = 7,
        .pipe_path = "/tmp/i2c_sim"
    };
    
    // Initialize and open device
    i2c_init(&device, &config);
    i2c_open(&device);
    
    // Write data
    uint8_t write_data[] = {0x01, 0x02, 0x03};
    i2c_write(&device, 0x50, write_data, sizeof(write_data));
    
    // Read data
    uint8_t read_data[4];
    i2c_read(&device, 0x50, read_data, sizeof(read_data));
    
    // Cleanup
    i2c_close(&device);
    return 0;
}
```

## Test Coverage

The test suite covers:

- **Basic Operations**: Simple read/write transactions
- **Error Handling**: NACK responses, invalid addresses, timeouts
- **Data Integrity**: Write-read cycles with data verification
- **Performance**: Transaction rate measurements
- **Protocol Compliance**: Multi-message transfers, start/stop conditions
- **Edge Cases**: Large transfers, boundary conditions

## Configuration

### Default Slave Devices

The bridge simulates several I2C slave devices:

- `0x50`: 256-byte EEPROM-like device
- `0x51`: 128-byte sensor-like device  
- `0x52`: 64-byte small device

### Timeouts and Timing

- Default transaction timeout: 1000ms
- Bridge polling interval: 1ms
- Simulation delays: 10ms per transaction

## Troubleshooting

### Common Issues

1. **"Failed to open pipes"**
   - Make sure the bridge is running first
   - Check that `/tmp/i2c_sim_tx` and `/tmp/i2c_sim_rx` exist

2. **"RTL simulation not found"**
   - Build the RTL simulation first: `make simulate`
   - Ensure `./sim/tb_i2c_master` is executable

3. **"Transaction timeout"**
   - Increase timeout: `i2c_set_timeout(&device, 5000)`
   - Check bridge and simulation are running

4. **"Permission denied"**
   - Check pipe permissions: `ls -la /tmp/i2c_sim*`
   - Try running with different user permissions

### Debug Mode

Enable debug output for detailed tracing:

```c
i2c_debug_enable(true);  // In code
```

Or in interactive mode:
```
i2c> debug on
```

## Performance

Typical performance on modern macOS systems:

- **Write transactions**: ~100-200 trans/sec
- **Read transactions**: ~80-150 trans/sec
- **Combined transfers**: ~50-100 trans/sec

Performance depends on:
- System load and RTL simulation speed
- Pipe buffer sizes and OS scheduling
- Transaction size and complexity

## Integration

### With RTL Testbench

The driver integrates with the existing RTL testbench:

1. RTL testbench runs in simulation
2. Bridge translates driver commands to RTL signals
3. Simulated slave devices provide responses
4. Results are communicated back to driver

### With Real Hardware

To adapt for real hardware:

1. Replace pipe communication with hardware interface (SPI, PCIe, etc.)
2. Modify register access functions in `send_command()/read_response()`
3. Update timing parameters for hardware speeds
4. Add hardware-specific initialization

## Files Structure

```
driver/
├── Makefile                    # Build system
├── README.md                   # This file
├── include/
│   └── i2c_driver.h           # Driver API header
├── i2c_driver.c               # Core driver implementation
├── i2c_bridge.c               # RTL simulation bridge
├── test/
│   ├── test_i2c_driver.c      # Automated test suite
│   └── interactive_test.c     # Interactive test tool
└── build/                     # Build output directory
    ├── libi2c_driver.a        # Driver library
    ├── i2c_bridge             # Bridge executable
    ├── test_i2c_driver        # Test suite
    └── interactive_test       # Interactive tool
```

## License

This I2C driver test suite is part of the I2C master controller RTL project. See the main project README for license information.
