// ============================================================================
// Simple I2C Driver Test
// ============================================================================
// Description: Basic test to verify I2C driver functionality
// ============================================================================

#include "../include/i2c_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Simple I2C Driver Test\n");
    printf("======================\n");
    
    // Enable debug output
    i2c_debug_enable(true);
    
    // Initialize I2C device
    i2c_device_t device;
    i2c_config_t config = {
        .clock_freq = 50000000,     // 50 MHz
        .i2c_freq = 100000,         // 100 kHz
        .addr_width = 7,            // 7-bit addressing
        .pipe_path = "/tmp/i2c_sim"
    };
    
    printf("\n1. Initializing I2C driver...\n");
    int ret = i2c_init(&device, &config);
    if (ret != I2C_SUCCESS) {
        printf("Failed to initialize I2C driver: %s\n", i2c_error_string(ret));
        return 1;
    }
    printf("Driver initialized successfully\n");
    
    printf("\n2. Opening I2C device...\n");
    ret = i2c_open(&device);
    if (ret != I2C_SUCCESS) {
        printf("Failed to open I2C device: %s\n", i2c_error_string(ret));
        printf("Make sure the I2C bridge is running!\n");
        printf("Run: make start-bridge\n");
        return 1;
    }
    printf("Device opened successfully\n");
    
    printf("\n3. Testing basic write operation...\n");
    uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04};
    ret = i2c_write(&device, 0x50, write_data, sizeof(write_data));
    if (ret >= 0) {
        printf("Write successful: %d bytes written\n", ret);
    } else {
        printf("Write failed: %s\n", i2c_error_string(ret));
    }
    
    printf("\n4. Testing basic read operation...\n");
    uint8_t read_data[4];
    ret = i2c_read(&device, 0x50, read_data, sizeof(read_data));
    if (ret >= 0) {
        printf("Read successful: %d bytes read\n", ret);
        printf("Data: ");
        for (int i = 0; i < ret; i++) {
            printf("0x%02X ", read_data[i]);
        }
        printf("\n");
    } else {
        printf("Read failed: %s\n", i2c_error_string(ret));
    }
    
    printf("\n5. Testing device scan...\n");
    int devices_found = 0;
    printf("Scanning I2C bus...\n");
    for (int addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy = 0;
        ret = i2c_write(&device, addr, &dummy, 1);
        if (ret >= 0) {
            printf("Device found at address 0x%02X\n", addr);
            devices_found++;
        }
        if (devices_found >= 5) break; // Limit output
    }
    printf("Scan complete. Found %d devices.\n", devices_found);
    
    printf("\n6. Closing I2C device...\n");
    i2c_close(&device);
    printf("Device closed successfully\n");
    
    printf("\nTest completed!\n");
    return 0;
}
