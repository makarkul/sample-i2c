// ============================================================================
// I2C Driver Implementation
// ============================================================================
// Description: I2C driver implementation for macOS that communicates with
//              RTL simulation through named pipes. Provides Linux-compatible
//              device driver interface.
// ============================================================================

#include "include/i2c_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <poll.h>
#include <stdarg.h>

// ============================================================================
// Private Constants and Macros
// ============================================================================

#define DEBUG_PREFIX "[I2C_DRIVER] "
#define PIPE_TX_SUFFIX "_tx"
#define PIPE_RX_SUFFIX "_rx"

// ============================================================================
// Private Variables
// ============================================================================

static bool debug_enabled = false;

// ============================================================================
// Private Function Prototypes
// ============================================================================

static int create_pipes(const char *base_path);
static int wait_for_completion(i2c_device_t *device);
static int send_command(i2c_device_t *device, const i2c_regs_t *regs);
static int read_response(i2c_device_t *device, i2c_regs_t *regs);
static void debug_print(const char *format, ...);

// ============================================================================
// Public Function Implementations
// ============================================================================

int i2c_init(i2c_device_t *device, const i2c_config_t *config) {
    if (!device || !config) {
        return I2C_ERROR_INVALID;
    }

    memset(device, 0, sizeof(i2c_device_t));
    device->config = *config;
    device->timeout_ms = I2C_TIMEOUT_MS;
    device->is_open = false;
    device->fd_tx = -1;
    device->fd_rx = -1;

    debug_print("Initialized I2C device with config:\n");
    debug_print("  Clock freq: %u Hz\n", config->clock_freq);
    debug_print("  I2C freq: %u Hz\n", config->i2c_freq);
    debug_print("  Address width: %u bits\n", config->addr_width);
    debug_print("  Pipe path: %s\n", config->pipe_path);

    return I2C_SUCCESS;
}

int i2c_open(i2c_device_t *device) {
    if (!device) {
        return I2C_ERROR_INVALID;
    }

    if (device->is_open) {
        return I2C_SUCCESS;
    }

    // Create named pipes if they don't exist
    int ret = create_pipes(device->config.pipe_path);
    if (ret != I2C_SUCCESS) {
        debug_print("Failed to create pipes\n");
        return ret;
    }

    // Construct pipe paths
    char tx_path[256], rx_path[256];
    snprintf(tx_path, sizeof(tx_path), "%s%s", device->config.pipe_path, PIPE_TX_SUFFIX);
    snprintf(rx_path, sizeof(rx_path), "%s%s", device->config.pipe_path, PIPE_RX_SUFFIX);

    // Open transmit pipe (write-only)
    device->fd_tx = open(tx_path, O_WRONLY | O_NONBLOCK);
    if (device->fd_tx < 0) {
        debug_print("Failed to open TX pipe: %s\n", strerror(errno));
        return I2C_ERROR_IO;
    }

    // Open receive pipe (read-only)
    device->fd_rx = open(rx_path, O_RDONLY | O_NONBLOCK);
    if (device->fd_rx < 0) {
        debug_print("Failed to open RX pipe: %s\n", strerror(errno));
        close(device->fd_tx);
        device->fd_tx = -1;
        return I2C_ERROR_IO;
    }

    device->is_open = true;
    debug_print("Opened I2C device successfully\n");

    return I2C_SUCCESS;
}

int i2c_close(i2c_device_t *device) {
    if (!device) {
        return I2C_ERROR_INVALID;
    }

    if (device->fd_tx >= 0) {
        close(device->fd_tx);
        device->fd_tx = -1;
    }

    if (device->fd_rx >= 0) {
        close(device->fd_rx);
        device->fd_rx = -1;
    }

    device->is_open = false;
    debug_print("Closed I2C device\n");

    return I2C_SUCCESS;
}

int i2c_transfer(i2c_device_t *device, i2c_msg_t *msgs, int num_msgs) {
    if (!device || !msgs || num_msgs <= 0) {
        return I2C_ERROR_INVALID;
    }

    if (!device->is_open) {
        return I2C_ERROR_IO;
    }

    debug_print("Starting I2C transfer with %d messages\n", num_msgs);
    i2c_debug_print_transfer(msgs, num_msgs);

    for (int i = 0; i < num_msgs; i++) {
        i2c_msg_t *msg = &msgs[i];
        i2c_regs_t regs = {0};

        // Setup address
        regs.addr = msg->addr;

        // Setup control register
        regs.control = 0;
        if (!(msg->flags & I2C_M_NOSTART)) {
            regs.control |= I2C_CTRL_START;
        }
        if (!(msg->flags & I2C_M_NOSTOP) && (i == num_msgs - 1)) {
            regs.control |= I2C_CTRL_STOP;
        }

        if (msg->flags & I2C_M_RD) {
            // Read transaction
            regs.control |= I2C_CTRL_READ;
            
            for (int j = 0; j < msg->len; j++) {
                int ret = send_command(device, &regs);
                if (ret != I2C_SUCCESS) {
                    return ret;
                }

                ret = wait_for_completion(device);
                if (ret != I2C_SUCCESS) {
                    return ret;
                }

                i2c_regs_t response;
                ret = read_response(device, &response);
                if (ret != I2C_SUCCESS) {
                    return ret;
                }

                if (response.status & I2C_STAT_ERROR) {
                    debug_print("I2C error during read\n");
                    return I2C_ERROR_NACK;
                }

                msg->buf[j] = response.data_rx & 0xFF;
                
                // Clear start for subsequent bytes
                regs.control &= ~I2C_CTRL_START;
            }
        } else {
            // Write transaction
            regs.control |= I2C_CTRL_WRITE;
            
            for (int j = 0; j < msg->len; j++) {
                regs.data_tx = msg->buf[j];
                
                int ret = send_command(device, &regs);
                if (ret != I2C_SUCCESS) {
                    return ret;
                }

                ret = wait_for_completion(device);
                if (ret != I2C_SUCCESS) {
                    return ret;
                }

                i2c_regs_t response;
                ret = read_response(device, &response);
                if (ret != I2C_SUCCESS) {
                    return ret;
                }

                if (response.status & I2C_STAT_ERROR) {
                    debug_print("I2C error during write\n");
                    return I2C_ERROR_NACK;
                }
                
                // Clear start for subsequent bytes
                regs.control &= ~I2C_CTRL_START;
            }
        }
    }

    debug_print("I2C transfer completed successfully\n");
    return num_msgs;
}

int i2c_read(i2c_device_t *device, uint16_t addr, uint8_t *buf, size_t len) {
    i2c_msg_t msg = {
        .addr = addr,
        .flags = I2C_M_RD,
        .len = len,
        .buf = buf
    };

    int ret = i2c_transfer(device, &msg, 1);
    return (ret == 1) ? len : ret;
}

int i2c_write(i2c_device_t *device, uint16_t addr, const uint8_t *buf, size_t len) {
    i2c_msg_t msg = {
        .addr = addr,
        .flags = 0,
        .len = len,
        .buf = (uint8_t *)buf
    };

    int ret = i2c_transfer(device, &msg, 1);
    return (ret == 1) ? len : ret;
}

int i2c_write_read(i2c_device_t *device, uint16_t addr, 
                   const uint8_t *write_buf, size_t write_len,
                   uint8_t *read_buf, size_t read_len) {
    i2c_msg_t msgs[2] = {
        {
            .addr = addr,
            .flags = 0,
            .len = write_len,
            .buf = (uint8_t *)write_buf
        },
        {
            .addr = addr,
            .flags = I2C_M_RD,
            .len = read_len,
            .buf = read_buf
        }
    };

    int ret = i2c_transfer(device, msgs, 2);
    return (ret == 2) ? I2C_SUCCESS : ret;
}

void i2c_set_timeout(i2c_device_t *device, uint32_t timeout_ms) {
    if (device) {
        device->timeout_ms = timeout_ms;
        debug_print("Set I2C timeout to %u ms\n", timeout_ms);
    }
}

uint32_t i2c_get_status(i2c_device_t *device) {
    if (!device || !device->is_open) {
        return 0;
    }

    i2c_regs_t regs = {0};
    if (read_response(device, &regs) == I2C_SUCCESS) {
        return regs.status;
    }
    return 0;
}

int i2c_reset(i2c_device_t *device) {
    if (!device || !device->is_open) {
        return I2C_ERROR_INVALID;
    }

    i2c_regs_t regs = {0};
    regs.control = I2C_CTRL_RESET;

    int ret = send_command(device, &regs);
    if (ret == I2C_SUCCESS) {
        debug_print("I2C controller reset\n");
    }

    return ret;
}

const char *i2c_error_string(int error) {
    switch (error) {
        case I2C_SUCCESS:     return "Success";
        case I2C_ERROR_TIMEOUT: return "Timeout";
        case I2C_ERROR_NACK:   return "No acknowledge";
        case I2C_ERROR_INVALID: return "Invalid parameter";
        case I2C_ERROR_BUSY:   return "Device busy";
        case I2C_ERROR_IO:     return "I/O error";
        default:              return "Unknown error";
    }
}

void i2c_debug_enable(bool enable) {
    debug_enabled = enable;
    debug_print("Debug %s\n", enable ? "enabled" : "disabled");
}

void i2c_debug_print_transfer(const i2c_msg_t *msgs, int num_msgs) {
    if (!debug_enabled || !msgs) return;

    for (int i = 0; i < num_msgs; i++) {
        const i2c_msg_t *msg = &msgs[i];
        debug_print("Message %d: addr=0x%02X, flags=0x%04X, len=%u\n",
                   i, msg->addr, msg->flags, msg->len);
        
        if (msg->len > 0 && msg->buf) {
            debug_print("  Data: ");
            for (int j = 0; j < msg->len && j < 16; j++) {
                printf("0x%02X ", msg->buf[j]);
            }
            if (msg->len > 16) {
                printf("... (%u bytes total)", msg->len);
            }
            printf("\n");
        }
    }
}

// ============================================================================
// Private Function Implementations
// ============================================================================

static int create_pipes(const char *base_path) {
    char tx_path[256], rx_path[256];
    snprintf(tx_path, sizeof(tx_path), "%s%s", base_path, PIPE_TX_SUFFIX);
    snprintf(rx_path, sizeof(rx_path), "%s%s", base_path, PIPE_RX_SUFFIX);

    // Create TX pipe
    if (mkfifo(tx_path, 0666) < 0 && errno != EEXIST) {
        debug_print("Failed to create TX pipe: %s\n", strerror(errno));
        return I2C_ERROR_IO;
    }

    // Create RX pipe
    if (mkfifo(rx_path, 0666) < 0 && errno != EEXIST) {
        debug_print("Failed to create RX pipe: %s\n", strerror(errno));
        return I2C_ERROR_IO;
    }

    debug_print("Created pipes: %s, %s\n", tx_path, rx_path);
    return I2C_SUCCESS;
}

static int wait_for_completion(i2c_device_t *device) {
    struct pollfd pfd = {
        .fd = device->fd_rx,
        .events = POLLIN,
        .revents = 0
    };

    int ret = poll(&pfd, 1, device->timeout_ms);
    if (ret < 0) {
        debug_print("Poll error: %s\n", strerror(errno));
        return I2C_ERROR_IO;
    } else if (ret == 0) {
        debug_print("Transaction timeout\n");
        return I2C_ERROR_TIMEOUT;
    }

    return I2C_SUCCESS;
}

static int send_command(i2c_device_t *device, const i2c_regs_t *regs) {
    ssize_t bytes_written = write(device->fd_tx, regs, sizeof(*regs));
    if (bytes_written != sizeof(*regs)) {
        debug_print("Failed to send command: %s\n", strerror(errno));
        return I2C_ERROR_IO;
    }

    debug_print("Sent command: ctrl=0x%08X, addr=0x%08X, data=0x%08X\n",
               regs->control, regs->addr, regs->data_tx);
    return I2C_SUCCESS;
}

static int read_response(i2c_device_t *device, i2c_regs_t *regs) {
    ssize_t bytes_read = read(device->fd_rx, regs, sizeof(*regs));
    if (bytes_read != sizeof(*regs)) {
        debug_print("Failed to read response: %s\n", strerror(errno));
        return I2C_ERROR_IO;
    }

    debug_print("Received response: status=0x%08X, data=0x%08X\n",
               regs->status, regs->data_rx);
    return I2C_SUCCESS;
}

static void debug_print(const char *format, ...) {
    if (!debug_enabled) return;

    printf(DEBUG_PREFIX);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
