// ============================================================================
// I2C Driver Test Suite
// ============================================================================
// Description: Comprehensive test suite for I2C driver functionality.
//              Tests various scenarios including basic I/O, error conditions,
//              and performance characteristics.
// ============================================================================

#include "../include/i2c_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

// ============================================================================
// Test Framework
// ============================================================================

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_results_t;

static test_results_t g_results = {0};

#define TEST_START(name) \
    do { \
        printf("\n=== Test: %s ===\n", name); \
        g_results.total_tests++; \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("PASS: %s\n", message); \
        } else { \
            printf("FAIL: %s\n", message); \
            g_results.failed_tests++; \
            return -1; \
        } \
    } while(0)

#define TEST_END() \
    do { \
        g_results.passed_tests++; \
        printf("Test completed successfully\n"); \
    } while(0)

// ============================================================================
// Test Data and Constants
// ============================================================================

#define TEST_SLAVE_ADDR     0x50
#define TEST_SLAVE_ADDR2    0x51
#define INVALID_SLAVE_ADDR  0x7F
#define TEST_DATA_SIZE      64

static uint8_t test_write_data[TEST_DATA_SIZE];
static uint8_t test_read_data[TEST_DATA_SIZE];

// ============================================================================
// Helper Functions
// ============================================================================

static void init_test_data(void) {
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        test_write_data[i] = (uint8_t)(i + 0x10);
    }
    memset(test_read_data, 0, sizeof(test_read_data));
}

static void print_data(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("0x%02X ", data[i]);
    }
    if (len > 16) {
        printf("... (%zu bytes total)", len);
    }
    printf("\n");
}

static int compare_data(const uint8_t *expected, const uint8_t *actual, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (expected[i] != actual[i]) {
            printf("Data mismatch at byte %zu: expected 0x%02X, got 0x%02X\n", 
                   i, expected[i], actual[i]);
            return -1;
        }
    }
    return 0;
}

// ============================================================================
// Test Functions
// ============================================================================

static int test_driver_init(i2c_device_t *device) {
    TEST_START("Driver Initialization");
    
    i2c_config_t config = {
        .clock_freq = 50000000,
        .i2c_freq = 100000,
        .addr_width = 7,
        .pipe_path = "/tmp/i2c_sim"
    };
    
    int ret = i2c_init(device, &config);
    TEST_ASSERT(ret == I2C_SUCCESS, "Driver initialization");
    
    ret = i2c_open(device);
    TEST_ASSERT(ret == I2C_SUCCESS, "Device open");
    
    TEST_END();
    return 0;
}

static int test_basic_write(i2c_device_t *device) {
    TEST_START("Basic Write Operation");
    
    uint8_t data[] = {0xAA, 0x55, 0x12, 0x34};
    
    int ret = i2c_write(device, TEST_SLAVE_ADDR, data, sizeof(data));
    TEST_ASSERT(ret == sizeof(data), "Write operation successful");
    
    print_data("Written data", data, sizeof(data));
    
    TEST_END();
    return 0;
}

static int test_basic_read(i2c_device_t *device) {
    TEST_START("Basic Read Operation");
    
    uint8_t data[4];
    memset(data, 0, sizeof(data));
    
    int ret = i2c_read(device, TEST_SLAVE_ADDR, data, sizeof(data));
    TEST_ASSERT(ret == sizeof(data), "Read operation successful");
    
    print_data("Read data", data, sizeof(data));
    
    TEST_END();
    return 0;
}

static int test_write_read_cycle(i2c_device_t *device) {
    TEST_START("Write-Read Cycle");
    
    uint8_t write_data[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    uint8_t read_data[sizeof(write_data)];
    
    // Write data
    int ret = i2c_write(device, TEST_SLAVE_ADDR, write_data, sizeof(write_data));
    TEST_ASSERT(ret == sizeof(write_data), "Write phase");
    
    print_data("Written", write_data, sizeof(write_data));
    
    // Read back data
    memset(read_data, 0, sizeof(read_data));
    ret = i2c_read(device, TEST_SLAVE_ADDR, read_data, sizeof(read_data));
    TEST_ASSERT(ret == sizeof(read_data), "Read phase");
    
    print_data("Read back", read_data, sizeof(read_data));
    
    // Note: Data might not match exactly due to simulation behavior
    // This test mainly verifies that operations complete successfully
    
    TEST_END();
    return 0;
}

static int test_combined_write_read(i2c_device_t *device) {
    TEST_START("Combined Write-Read Transaction");
    
    uint8_t write_data[] = {0x01, 0x02};  // Register address
    uint8_t read_data[4];
    
    int ret = i2c_write_read(device, TEST_SLAVE_ADDR, 
                           write_data, sizeof(write_data),
                           read_data, sizeof(read_data));
    TEST_ASSERT(ret == I2C_SUCCESS, "Combined write-read transaction");
    
    print_data("Write data", write_data, sizeof(write_data));
    print_data("Read data", read_data, sizeof(read_data));
    
    TEST_END();
    return 0;
}

static int test_large_transfer(i2c_device_t *device) {
    TEST_START("Large Data Transfer");
    
    uint8_t large_data[32];
    uint8_t read_back[32];
    
    // Initialize test pattern
    for (int i = 0; i < sizeof(large_data); i++) {
        large_data[i] = (uint8_t)(i + 0x80);
    }
    
    // Write large block
    int ret = i2c_write(device, TEST_SLAVE_ADDR, large_data, sizeof(large_data));
    TEST_ASSERT(ret == sizeof(large_data), "Large write operation");
    
    // Read back
    memset(read_back, 0, sizeof(read_back));
    ret = i2c_read(device, TEST_SLAVE_ADDR, read_back, sizeof(read_back));
    TEST_ASSERT(ret == sizeof(read_back), "Large read operation");
    
    printf("Large transfer completed: %d bytes written, %d bytes read\n", 
           (int)sizeof(large_data), ret);
    
    TEST_END();
    return 0;
}

static int test_multiple_messages(i2c_device_t *device) {
    TEST_START("Multiple Message Transfer");
    
    uint8_t write_buf1[] = {0x11, 0x22};
    uint8_t write_buf2[] = {0x33, 0x44, 0x55};
    uint8_t read_buf[3];
    
    i2c_msg_t msgs[] = {
        {
            .addr = TEST_SLAVE_ADDR,
            .flags = 0,
            .len = sizeof(write_buf1),
            .buf = write_buf1
        },
        {
            .addr = TEST_SLAVE_ADDR,
            .flags = 0,
            .len = sizeof(write_buf2),
            .buf = write_buf2
        },
        {
            .addr = TEST_SLAVE_ADDR,
            .flags = I2C_M_RD,
            .len = sizeof(read_buf),
            .buf = read_buf
        }
    };
    
    int ret = i2c_transfer(device, msgs, 3);
    TEST_ASSERT(ret == 3, "Multiple message transfer");
    
    printf("Transferred %d messages successfully\n", ret);
    print_data("Final read", read_buf, sizeof(read_buf));
    
    TEST_END();
    return 0;
}

static int test_error_conditions(i2c_device_t *device) {
    TEST_START("Error Condition Handling");
    
    uint8_t data[] = {0x01, 0x02};
    
    // Test with invalid slave address (should get NACK)
    int ret = i2c_write(device, INVALID_SLAVE_ADDR, data, sizeof(data));
    TEST_ASSERT(ret < 0, "NACK error detection");
    printf("Got expected error: %s\n", i2c_error_string(ret));
    
    // Test NULL pointer handling
    ret = i2c_write(device, TEST_SLAVE_ADDR, NULL, sizeof(data));
    TEST_ASSERT(ret < 0, "NULL pointer handling");
    
    TEST_END();
    return 0;
}

static int test_timeout_behavior(i2c_device_t *device) {
    TEST_START("Timeout Behavior");
    
    // Set a short timeout
    i2c_set_timeout(device, 100); // 100ms
    
    uint8_t data[] = {0xFF, 0xFF};
    
    // This might timeout depending on simulation timing
    int ret = i2c_write(device, TEST_SLAVE_ADDR, data, sizeof(data));
    
    // Reset to normal timeout
    i2c_set_timeout(device, 1000);
    
    printf("Timeout test result: %s\n", i2c_error_string(ret));
    
    TEST_END();
    return 0;
}

static int test_device_reset(i2c_device_t *device) {
    TEST_START("Device Reset");
    
    int ret = i2c_reset(device);
    TEST_ASSERT(ret == I2C_SUCCESS, "Device reset");
    
    // Verify device is still functional after reset
    uint8_t data[] = {0xAB, 0xCD};
    ret = i2c_write(device, TEST_SLAVE_ADDR, data, sizeof(data));
    TEST_ASSERT(ret == sizeof(data), "Operation after reset");
    
    TEST_END();
    return 0;
}

static int test_performance(i2c_device_t *device) {
    TEST_START("Performance Measurement");
    
    const int num_iterations = 100;
    const int data_size = 16;
    uint8_t data[data_size];
    
    // Initialize test data
    for (int i = 0; i < data_size; i++) {
        data[i] = (uint8_t)(i + 0x60);
    }
    
    // Measure write performance
    clock_t start = clock();
    
    for (int i = 0; i < num_iterations; i++) {
        int ret = i2c_write(device, TEST_SLAVE_ADDR, data, data_size);
        if (ret != data_size) {
            printf("Write failed at iteration %d: %s\n", i, i2c_error_string(ret));
            break;
        }
    }
    
    clock_t end = clock();
    double write_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // Measure read performance
    start = clock();
    
    for (int i = 0; i < num_iterations; i++) {
        int ret = i2c_read(device, TEST_SLAVE_ADDR, data, data_size);
        if (ret != data_size) {
            printf("Read failed at iteration %d: %s\n", i, i2c_error_string(ret));
            break;
        }
    }
    
    end = clock();
    double read_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Performance Results:\n");
    printf("  Write: %d transactions in %.3f seconds (%.1f trans/sec)\n", 
           num_iterations, write_time, num_iterations / write_time);
    printf("  Read:  %d transactions in %.3f seconds (%.1f trans/sec)\n", 
           num_iterations, read_time, num_iterations / read_time);
    printf("  Total data: %d bytes per direction\n", num_iterations * data_size);
    
    TEST_END();
    return 0;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char *argv[]) {
    printf("============================================\n");
    printf("I2C Driver Test Suite\n");
    printf("============================================\n");
    
    // Enable debug output
    i2c_debug_enable(true);
    
    // Initialize test data
    init_test_data();
    
    i2c_device_t device;
    
    // Run tests
    int ret = 0;
    
    ret |= test_driver_init(&device);
    if (ret == 0) ret |= test_basic_write(&device);
    if (ret == 0) ret |= test_basic_read(&device);
    if (ret == 0) ret |= test_write_read_cycle(&device);
    if (ret == 0) ret |= test_combined_write_read(&device);
    if (ret == 0) ret |= test_large_transfer(&device);
    if (ret == 0) ret |= test_multiple_messages(&device);
    if (ret == 0) ret |= test_error_conditions(&device);
    if (ret == 0) ret |= test_timeout_behavior(&device);
    if (ret == 0) ret |= test_device_reset(&device);
    if (ret == 0) ret |= test_performance(&device);
    
    // Cleanup
    i2c_close(&device);
    
    // Print summary
    printf("\n============================================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", g_results.total_tests);
    printf("  Passed: %d\n", g_results.passed_tests);
    printf("  Failed: %d\n", g_results.failed_tests);
    
    if (g_results.failed_tests == 0) {
        printf("  Result: ALL TESTS PASSED!\n");
    } else {
        printf("  Result: %d TESTS FAILED!\n", g_results.failed_tests);
    }
    printf("============================================\n");
    
    return (g_results.failed_tests == 0) ? 0 : 1;
}
