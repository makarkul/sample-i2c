// ============================================================================
// I2C Driver Interactive Test
// ============================================================================
// Description: Interactive command-line tool for testing I2C driver
//              functionality. Provides a shell-like interface for manual
//              testing and debugging.
// ============================================================================

#include "../include/i2c_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// ============================================================================
// Constants and Definitions
// ============================================================================

#define MAX_COMMAND_LEN     256
#define MAX_DATA_LEN        64
#define PROMPT             "i2c> "

// ============================================================================
// Global Variables
// ============================================================================

static i2c_device_t g_device;
static bool g_device_open = false;

// ============================================================================
// Helper Functions
// ============================================================================

static void print_help(void) {
    printf("\nI2C Driver Interactive Test Commands:\n");
    printf("  help                     - Show this help\n");
    printf("  init                     - Initialize I2C driver\n");
    printf("  open                     - Open I2C device\n");
    printf("  close                    - Close I2C device\n");
    printf("  status                   - Show device status\n");
    printf("  reset                    - Reset I2C controller\n");
    printf("  write <addr> <data...>   - Write data to slave address\n");
    printf("  read <addr> <length>     - Read data from slave address\n");
    printf("  scan                     - Scan for I2C devices\n");
    printf("  timeout <ms>             - Set transaction timeout\n");
    printf("  debug <on|off>           - Enable/disable debug output\n");
    printf("  quit                     - Exit the program\n");
    printf("\nExamples:\n");
    printf("  write 0x50 0x01 0x02 0x03\n");
    printf("  read 0x50 4\n");
    printf("  timeout 2000\n");
    printf("\n");
}

static int parse_hex_byte(const char *str) {
    char *endptr;
    long val = strtol(str, &endptr, 0);
    
    if (*endptr != '\0' || val < 0 || val > 255) {
        return -1;
    }
    
    return (int)val;
}

static void print_data_hex(const uint8_t *data, size_t len) {
    printf("Data (%zu bytes): ", len);
    for (size_t i = 0; i < len; i++) {
        printf("0x%02X ", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len) {
            printf("\n                  ");
        }
    }
    printf("\n");
}

static int cmd_init(void) {
    i2c_config_t config = {
        .clock_freq = 50000000,     // 50 MHz
        .i2c_freq = 100000,         // 100 kHz
        .addr_width = 7,            // 7-bit addressing
        .pipe_path = "/tmp/i2c_sim"
    };
    
    int ret = i2c_init(&g_device, &config);
    if (ret == I2C_SUCCESS) {
        printf("I2C driver initialized successfully\n");
    } else {
        printf("Failed to initialize I2C driver: %s\n", i2c_error_string(ret));
    }
    
    return ret;
}

static int cmd_open(void) {
    if (g_device_open) {
        printf("Device is already open\n");
        return I2C_SUCCESS;
    }
    
    int ret = i2c_open(&g_device);
    if (ret == I2C_SUCCESS) {
        g_device_open = true;
        printf("I2C device opened successfully\n");
    } else {
        printf("Failed to open I2C device: %s\n", i2c_error_string(ret));
    }
    
    return ret;
}

static int cmd_close(void) {
    if (!g_device_open) {
        printf("Device is not open\n");
        return I2C_SUCCESS;
    }
    
    int ret = i2c_close(&g_device);
    if (ret == I2C_SUCCESS) {
        g_device_open = false;
        printf("I2C device closed successfully\n");
    } else {
        printf("Failed to close I2C device: %s\n", i2c_error_string(ret));
    }
    
    return ret;
}

static int cmd_status(void) {
    if (!g_device_open) {
        printf("Device is not open\n");
        return I2C_ERROR_INVALID;
    }
    
    uint32_t status = i2c_get_status(&g_device);
    
    printf("I2C Device Status: 0x%08X\n", status);
    printf("  Busy:       %s\n", (status & I2C_STAT_BUSY) ? "Yes" : "No");
    printf("  ACK:        %s\n", (status & I2C_STAT_ACK) ? "Yes" : "No");
    printf("  Error:      %s\n", (status & I2C_STAT_ERROR) ? "Yes" : "No");
    printf("  Timeout:    %s\n", (status & I2C_STAT_TIMEOUT) ? "Yes" : "No");
    printf("  Data Valid: %s\n", (status & I2C_STAT_DATA_VALID) ? "Yes" : "No");
    
    return I2C_SUCCESS;
}

static int cmd_reset(void) {
    if (!g_device_open) {
        printf("Device is not open\n");
        return I2C_ERROR_INVALID;
    }
    
    int ret = i2c_reset(&g_device);
    if (ret == I2C_SUCCESS) {
        printf("I2C controller reset successfully\n");
    } else {
        printf("Failed to reset I2C controller: %s\n", i2c_error_string(ret));
    }
    
    return ret;
}

static int cmd_write(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: write <addr> <data...>\n");
        return I2C_ERROR_INVALID;
    }
    
    if (!g_device_open) {
        printf("Device is not open\n");
        return I2C_ERROR_INVALID;
    }
    
    // Parse address
    int addr = parse_hex_byte(argv[1]);
    if (addr < 0) {
        printf("Invalid address: %s\n", argv[1]);
        return I2C_ERROR_INVALID;
    }
    
    // Parse data bytes
    uint8_t data[MAX_DATA_LEN];
    int data_len = 0;
    
    for (int i = 2; i < argc && data_len < MAX_DATA_LEN; i++) {
        int byte_val = parse_hex_byte(argv[i]);
        if (byte_val < 0) {
            printf("Invalid data byte: %s\n", argv[i]);
            return I2C_ERROR_INVALID;
        }
        data[data_len++] = (uint8_t)byte_val;
    }
    
    printf("Writing to address 0x%02X:\n", addr);
    print_data_hex(data, data_len);
    
    int ret = i2c_write(&g_device, (uint16_t)addr, data, data_len);
    if (ret >= 0) {
        printf("Write successful: %d bytes written\n", ret);
    } else {
        printf("Write failed: %s\n", i2c_error_string(ret));
    }
    
    return ret;
}

static int cmd_read(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: read <addr> <length>\n");
        return I2C_ERROR_INVALID;
    }
    
    if (!g_device_open) {
        printf("Device is not open\n");
        return I2C_ERROR_INVALID;
    }
    
    // Parse address
    int addr = parse_hex_byte(argv[1]);
    if (addr < 0) {
        printf("Invalid address: %s\n", argv[1]);
        return I2C_ERROR_INVALID;
    }
    
    // Parse length
    int length = atoi(argv[2]);
    if (length <= 0 || length > MAX_DATA_LEN) {
        printf("Invalid length: %s (must be 1-%d)\n", argv[2], MAX_DATA_LEN);
        return I2C_ERROR_INVALID;
    }
    
    uint8_t data[MAX_DATA_LEN];
    
    printf("Reading %d bytes from address 0x%02X:\n", length, addr);
    
    int ret = i2c_read(&g_device, (uint16_t)addr, data, length);
    if (ret >= 0) {
        printf("Read successful: %d bytes read\n", ret);
        print_data_hex(data, ret);
    } else {
        printf("Read failed: %s\n", i2c_error_string(ret));
    }
    
    return ret;
}

static int cmd_scan(void) {
    if (!g_device_open) {
        printf("Device is not open\n");
        return I2C_ERROR_INVALID;
    }
    
    printf("Scanning I2C bus (7-bit addresses):\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    
    int devices_found = 0;
    
    for (int addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) {
            printf("%02x: ", addr);
        }
        
        // Try to write a single byte to detect device presence
        uint8_t dummy = 0;
        int ret = i2c_write(&g_device, (uint16_t)addr, &dummy, 1);
        
        if (ret >= 0) {
            printf("%02x ", addr);
            devices_found++;
        } else {
            printf("-- ");
        }
        
        if (addr % 16 == 15) {
            printf("\n");
        }
    }
    
    printf("\nScan complete. Found %d devices.\n", devices_found);
    return I2C_SUCCESS;
}

static int cmd_timeout(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: timeout <milliseconds>\n");
        return I2C_ERROR_INVALID;
    }
    
    int timeout_ms = atoi(argv[1]);
    if (timeout_ms <= 0) {
        printf("Invalid timeout: %s\n", argv[1]);
        return I2C_ERROR_INVALID;
    }
    
    i2c_set_timeout(&g_device, (uint32_t)timeout_ms);
    printf("Timeout set to %d ms\n", timeout_ms);
    
    return I2C_SUCCESS;
}

static int cmd_debug(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: debug <on|off>\n");
        return I2C_ERROR_INVALID;
    }
    
    bool enable;
    if (strcmp(argv[1], "on") == 0) {
        enable = true;
    } else if (strcmp(argv[1], "off") == 0) {
        enable = false;
    } else {
        printf("Invalid debug setting: %s (use 'on' or 'off')\n", argv[1]);
        return I2C_ERROR_INVALID;
    }
    
    i2c_debug_enable(enable);
    printf("Debug output %s\n", enable ? "enabled" : "disabled");
    
    return I2C_SUCCESS;
}

static int process_command(char *line) {
    // Remove trailing newline
    char *newline = strchr(line, '\n');
    if (newline) *newline = '\0';
    
    // Skip empty lines and comments
    if (line[0] == '\0' || line[0] == '#') {
        return 0;
    }
    
    // Tokenize command line
    char *argv[32];
    int argc = 0;
    
    char *token = strtok(line, " \t");
    while (token && argc < 31) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    
    if (argc == 0) {
        return 0;
    }
    
    // Process commands
    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
        print_help();
        return 0;
    } else if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) {
        return 1;
    } else if (strcmp(argv[0], "init") == 0) {
        cmd_init();
    } else if (strcmp(argv[0], "open") == 0) {
        cmd_open();
    } else if (strcmp(argv[0], "close") == 0) {
        cmd_close();
    } else if (strcmp(argv[0], "status") == 0) {
        cmd_status();
    } else if (strcmp(argv[0], "reset") == 0) {
        cmd_reset();
    } else if (strcmp(argv[0], "write") == 0) {
        cmd_write(argc, argv);
    } else if (strcmp(argv[0], "read") == 0) {
        cmd_read(argc, argv);
    } else if (strcmp(argv[0], "scan") == 0) {
        cmd_scan();
    } else if (strcmp(argv[0], "timeout") == 0) {
        cmd_timeout(argc, argv);
    } else if (strcmp(argv[0], "debug") == 0) {
        cmd_debug(argc, argv);
    } else {
        printf("Unknown command: %s (type 'help' for available commands)\n", argv[0]);
    }
    
    return 0;
}

// ============================================================================
// Main Function
// ============================================================================

int main(int argc, char *argv[]) {
    printf("I2C Driver Interactive Test\n");
    printf("Type 'help' for available commands\n");
    printf("Make sure the I2C bridge is running before using this tool\n\n");
    
    char line[MAX_COMMAND_LEN];
    
    while (1) {
        printf(PROMPT);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break; // EOF
        }
        
        int result = process_command(line);
        if (result != 0) {
            break; // Quit command
        }
    }
    
    // Cleanup
    if (g_device_open) {
        i2c_close(&g_device);
    }
    
    printf("\nGoodbye!\n");
    return 0;
}
