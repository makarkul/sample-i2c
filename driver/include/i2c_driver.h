// ============================================================================
// I2C Driver Header
// ============================================================================
// Description: Header file for I2C driver interface compatible with Linux
//              device driver model. Provides hardware abstraction for I2C
//              master controller RTL.
// ============================================================================

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// ============================================================================
// Constants and Definitions
// ============================================================================

#define I2C_DEVICE_NAME     "i2c_master"
#define I2C_MAJOR_NUMBER    200
#define I2C_MAX_DEVICES     4

// I2C Transaction flags
#define I2C_M_RD            0x0001  // Read transaction
#define I2C_M_NOSTART       0x4000  // No start condition
#define I2C_M_NOSTOP        0x8000  // No stop condition

// Driver specific constants
#define I2C_MAX_MSG_LEN     256
#define I2C_TIMEOUT_MS      1000
#define I2C_PIPE_BUFFER     1024

// Error codes
#define I2C_SUCCESS         0
#define I2C_ERROR_TIMEOUT   -1
#define I2C_ERROR_NACK      -2
#define I2C_ERROR_INVALID   -3
#define I2C_ERROR_BUSY      -4
#define I2C_ERROR_IO        -5

// ============================================================================
// Data Structures
// ============================================================================

/**
 * I2C message structure - compatible with Linux i2c_msg
 */
typedef struct {
    uint16_t addr;      // Slave address
    uint16_t flags;     // Transaction flags
    uint16_t len;       // Message length
    uint8_t *buf;       // Data buffer
} i2c_msg_t;

/**
 * I2C adapter configuration
 */
typedef struct {
    uint32_t clock_freq;    // System clock frequency (Hz)
    uint32_t i2c_freq;      // I2C bus frequency (Hz)
    uint8_t addr_width;     // Address width (7 or 10 bits)
    char *pipe_path;        // Path to communication pipe
} i2c_config_t;

/**
 * I2C device context
 */
typedef struct {
    int fd_tx;              // Transmit pipe file descriptor
    int fd_rx;              // Receive pipe file descriptor
    i2c_config_t config;    // Device configuration
    bool is_open;           // Device state
    uint32_t timeout_ms;    // Transaction timeout
} i2c_device_t;

/**
 * I2C register interface (for RTL communication)
 */
typedef struct {
    uint32_t control;       // Control register
    uint32_t status;        // Status register
    uint32_t addr;          // Address register
    uint32_t data_tx;       // Transmit data register
    uint32_t data_rx;       // Receive data register
    uint32_t config;        // Configuration register
} i2c_regs_t;

// Control register bits
#define I2C_CTRL_START      (1 << 0)
#define I2C_CTRL_STOP       (1 << 1)
#define I2C_CTRL_READ       (1 << 2)
#define I2C_CTRL_WRITE      (1 << 3)
#define I2C_CTRL_RESET      (1 << 31)

// Status register bits
#define I2C_STAT_BUSY       (1 << 0)
#define I2C_STAT_ACK        (1 << 1)
#define I2C_STAT_ERROR      (1 << 2)
#define I2C_STAT_TIMEOUT    (1 << 3)
#define I2C_STAT_DATA_VALID (1 << 4)

// ============================================================================
// Function Prototypes
// ============================================================================

/**
 * Initialize I2C driver
 * @param device Pointer to I2C device structure
 * @param config Pointer to configuration structure
 * @return I2C_SUCCESS on success, error code on failure
 */
int i2c_init(i2c_device_t *device, const i2c_config_t *config);

/**
 * Open I2C device
 * @param device Pointer to I2C device structure
 * @return I2C_SUCCESS on success, error code on failure
 */
int i2c_open(i2c_device_t *device);

/**
 * Close I2C device
 * @param device Pointer to I2C device structure
 * @return I2C_SUCCESS on success, error code on failure
 */
int i2c_close(i2c_device_t *device);

/**
 * Transfer I2C messages (Linux-compatible interface)
 * @param device Pointer to I2C device structure
 * @param msgs Array of I2C messages
 * @param num_msgs Number of messages
 * @return Number of messages transferred on success, negative error code on failure
 */
int i2c_transfer(i2c_device_t *device, i2c_msg_t *msgs, int num_msgs);

/**
 * Read from I2C slave device
 * @param device Pointer to I2C device structure
 * @param addr Slave address
 * @param buf Buffer to store read data
 * @param len Number of bytes to read
 * @return Number of bytes read on success, negative error code on failure
 */
int i2c_read(i2c_device_t *device, uint16_t addr, uint8_t *buf, size_t len);

/**
 * Write to I2C slave device
 * @param device Pointer to I2C device structure
 * @param addr Slave address
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return Number of bytes written on success, negative error code on failure
 */
int i2c_write(i2c_device_t *device, uint16_t addr, const uint8_t *buf, size_t len);

/**
 * Combined write-read transaction
 * @param device Pointer to I2C device structure
 * @param addr Slave address
 * @param write_buf Buffer containing data to write
 * @param write_len Number of bytes to write
 * @param read_buf Buffer to store read data
 * @param read_len Number of bytes to read
 * @return I2C_SUCCESS on success, negative error code on failure
 */
int i2c_write_read(i2c_device_t *device, uint16_t addr, 
                   const uint8_t *write_buf, size_t write_len,
                   uint8_t *read_buf, size_t read_len);

/**
 * Set I2C timeout
 * @param device Pointer to I2C device structure
 * @param timeout_ms Timeout in milliseconds
 */
void i2c_set_timeout(i2c_device_t *device, uint32_t timeout_ms);

/**
 * Get I2C device status
 * @param device Pointer to I2C device structure
 * @return Status register value
 */
uint32_t i2c_get_status(i2c_device_t *device);

/**
 * Reset I2C controller
 * @param device Pointer to I2C device structure
 * @return I2C_SUCCESS on success, negative error code on failure
 */
int i2c_reset(i2c_device_t *device);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert error code to string
 * @param error Error code
 * @return String description of error
 */
const char *i2c_error_string(int error);

/**
 * Enable debug logging
 * @param enable True to enable, false to disable
 */
void i2c_debug_enable(bool enable);

/**
 * Print I2C transaction for debugging
 * @param msgs Array of I2C messages
 * @param num_msgs Number of messages
 */
void i2c_debug_print_transfer(const i2c_msg_t *msgs, int num_msgs);

#endif // I2C_DRIVER_H
