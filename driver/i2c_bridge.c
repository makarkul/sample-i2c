// ============================================================================
// I2C RTL Simulation Bridge
// ============================================================================
// Description: Bridge between I2C driver and RTL simulation using named pipes.
//              Translates driver commands to RTL signals and responses back.
// ============================================================================

#include "include/i2c_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>

// ============================================================================
// Constants and Definitions
// ============================================================================

#define BRIDGE_NAME         "[I2C_BRIDGE] "
#define SIM_EXECUTABLE      "../sim/tb_i2c_master"
#define VCD_FILE           "../sim/i2c_master.vcd"
#define MAX_SLAVES          16

// ============================================================================
// Data Structures
// ============================================================================

typedef struct {
    uint16_t addr;
    uint8_t *memory;
    size_t memory_size;
    bool enabled;
} i2c_slave_device_t;

typedef struct {
    int fd_tx;              // Pipe from driver
    int fd_rx;              // Pipe to driver
    pid_t sim_pid;          // RTL simulation process ID
    bool running;           // Bridge running state
    pthread_t worker_thread; // Worker thread
    i2c_slave_device_t slaves[MAX_SLAVES]; // Simulated slave devices
    int num_slaves;
} bridge_context_t;

// ============================================================================
// Global Variables
// ============================================================================

static bridge_context_t g_bridge = {0};
static volatile bool g_shutdown = false;

// ============================================================================
// Function Prototypes
// ============================================================================

static void signal_handler(int sig);
static int start_rtl_simulation(void);
static void stop_rtl_simulation(void);
static void *bridge_worker(void *arg);
static int process_driver_command(const i2c_regs_t *cmd, i2c_regs_t *response);
static int simulate_i2c_transaction(uint16_t addr, bool is_read, uint8_t data, uint8_t *response_data);
static void add_slave_device(uint16_t addr, size_t memory_size);
static i2c_slave_device_t *find_slave_device(uint16_t addr);
static void cleanup_bridge(void);

// ============================================================================
// Main Functions
// ============================================================================

// Forward declarations for daemon functions
static int daemonize(void);
static void write_pid_file(pid_t pid);
static pid_t read_pid_file(void);

#define PID_FILE "/tmp/i2c_bridge.pid"
#define LOG_FILE "/tmp/i2c_bridge.log"

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -d, --daemon    Run as daemon\n");
            printf("  -h, --help      Show this help\n");
            return 0;
        }
    }
    
    if (daemon_mode) {
        if (daemonize() != 0) {
            fprintf(stderr, "Failed to daemonize\n");
            return 1;
        }
        write_pid_file(getpid());
    } else {
        printf(BRIDGE_NAME "Starting I2C RTL Simulation Bridge\n");
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize bridge context
    memset(&g_bridge, 0, sizeof(g_bridge));
    g_bridge.fd_tx = -1;
    g_bridge.fd_rx = -1;
    g_bridge.sim_pid = -1;
    
    // Add some default slave devices for testing
    add_slave_device(0x50, 256);  // EEPROM-like device
    add_slave_device(0x51, 128);  // Sensor-like device
    add_slave_device(0x52, 64);   // Small device
    
    // Create named pipes
    const char *pipe_base = "/tmp/i2c_sim";
    char tx_path[256], rx_path[256];
    snprintf(tx_path, sizeof(tx_path), "%s_tx", pipe_base);
    snprintf(rx_path, sizeof(rx_path), "%s_rx", pipe_base);
    
    // Remove existing pipes
    unlink(tx_path);
    unlink(rx_path);
    
    // Create new pipes
    if (mkfifo(tx_path, 0666) < 0) {
        perror("mkfifo tx");
        return 1;
    }
    
    if (mkfifo(rx_path, 0666) < 0) {
        perror("mkfifo rx");
        unlink(tx_path);
        return 1;
    }
    
    printf(BRIDGE_NAME "Created pipes: %s, %s\n", tx_path, rx_path);
    
    // Start RTL simulation
    if (start_rtl_simulation() != 0) {
        fprintf(stderr, BRIDGE_NAME "Failed to start RTL simulation\n");
        cleanup_bridge();
        return 1;
    }
     // Open pipes in non-blocking mode to avoid hanging
    printf(BRIDGE_NAME "Opening pipes...\n");
    
    // For macOS, we need to open the pipes carefully
    // First, open TX pipe in non-blocking read mode
    g_bridge.fd_tx = open(tx_path, O_RDONLY | O_NONBLOCK);
    if (g_bridge.fd_tx < 0) {
        perror("open tx pipe");
        cleanup_bridge();
        return 1;
    }

    // Open RX pipe for writing, but this might block
    // Try non-blocking first, then fall back to blocking with timeout
    g_bridge.fd_rx = open(rx_path, O_WRONLY | O_NONBLOCK);
    if (g_bridge.fd_rx < 0) {
        if (errno == ENXIO) {
            // No reader yet, that's OK - we'll handle this in the worker thread
            printf(BRIDGE_NAME "RX pipe not ready yet (no reader), will retry\n");
            g_bridge.fd_rx = -1;
        } else {
            perror("open rx pipe");
            cleanup_bridge();
            return 1;
        }
    }
    
    printf(BRIDGE_NAME "Pipes opened successfully\n");
    
    // Start worker thread
    g_bridge.running = true;
    if (pthread_create(&g_bridge.worker_thread, NULL, bridge_worker, NULL) != 0) {
        perror("pthread_create");
        cleanup_bridge();
        return 1;
    }
    
    printf(BRIDGE_NAME "Bridge is running. Press Ctrl+C to stop.\n");
    printf(BRIDGE_NAME "Driver can connect to: %s\n", pipe_base);
    
    // Wait for shutdown signal
    while (!g_shutdown) {
        sleep(1);
        
        // Check if simulation is still running
        if (g_bridge.sim_pid > 0) {
            int status;
            pid_t result = waitpid(g_bridge.sim_pid, &status, WNOHANG);
            if (result > 0) {
                printf(BRIDGE_NAME "RTL simulation terminated\n");
                g_bridge.sim_pid = -1;
            }
        }
    }
    
    printf(BRIDGE_NAME "Shutting down...\n");
    cleanup_bridge();
    
    return 0;
}

// ============================================================================
// Implementation Functions
// ============================================================================

static void signal_handler(int sig) {
    printf(BRIDGE_NAME "Received signal %d, shutting down...\n", sig);
    g_shutdown = true;
}

static int start_rtl_simulation(void) {
    printf(BRIDGE_NAME "Starting RTL simulation...\n");
    
    // Check if simulation executable exists
    if (access(SIM_EXECUTABLE, X_OK) != 0) {
        printf(BRIDGE_NAME "Warning: Simulation executable not found: %s\n", SIM_EXECUTABLE);
        printf(BRIDGE_NAME "Running in simulation mode without RTL\n");
        return 0;  // Continue without simulation for now
    }
    
    // Fork and exec the simulation
    g_bridge.sim_pid = fork();
    if (g_bridge.sim_pid < 0) {
        perror("fork");
        return -1;
    } else if (g_bridge.sim_pid == 0) {
        // Child process - run simulation
        printf(BRIDGE_NAME "Executing RTL simulation\n");
        execlp(SIM_EXECUTABLE, SIM_EXECUTABLE, NULL);
        perror("execlp");
        exit(1);
    }
    
    // Parent process
    printf(BRIDGE_NAME "RTL simulation started with PID %d\n", g_bridge.sim_pid);
    
    // Give simulation time to start
    sleep(1);
    
    return 0;
}

static void stop_rtl_simulation(void) {
    if (g_bridge.sim_pid > 0) {
        printf(BRIDGE_NAME "Stopping RTL simulation (PID %d)\n", g_bridge.sim_pid);
        kill(g_bridge.sim_pid, SIGTERM);
        
        // Wait for process to terminate
        int status;
        waitpid(g_bridge.sim_pid, &status, 0);
        g_bridge.sim_pid = -1;
    }
}

static void *bridge_worker(void *arg) {
    printf(BRIDGE_NAME "Worker thread started\n");
    
    while (g_bridge.running && !g_shutdown) {
        i2c_regs_t cmd, response;
        
        // If RX pipe isn't open yet, try to open it
        if (g_bridge.fd_rx < 0) {
            g_bridge.fd_rx = open("/tmp/i2c_sim_rx", O_WRONLY | O_NONBLOCK);
            if (g_bridge.fd_rx < 0 && errno != ENXIO) {
                perror("open rx pipe in worker");
                break;
            }
        }
        
        // Read command from driver
        ssize_t bytes_read = read(g_bridge.fd_tx, &cmd, sizeof(cmd));
        if (bytes_read != sizeof(cmd)) {
            if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("read command");
                break;
            }
            usleep(1000); // 1ms delay
            continue;
        }
        
        printf(BRIDGE_NAME "Received command: ctrl=0x%08X, addr=0x%08X, data=0x%08X\n",
               cmd.control, cmd.addr, cmd.data_tx);
        
        // Process command
        memset(&response, 0, sizeof(response));
        int result = process_driver_command(&cmd, &response);
        
        if (result != I2C_SUCCESS) {
            response.status |= I2C_STAT_ERROR;
        }
        
        // Send response to driver (only if RX pipe is open)
        if (g_bridge.fd_rx >= 0) {
            ssize_t bytes_written = write(g_bridge.fd_rx, &response, sizeof(response));
            if (bytes_written != sizeof(response)) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    printf(BRIDGE_NAME "Driver disconnected, closing RX pipe\n");
                    close(g_bridge.fd_rx);
                    g_bridge.fd_rx = -1;
                } else {
                    perror("write response");
                    break;
                }
            } else {
                printf(BRIDGE_NAME "Sent response: status=0x%08X, data=0x%08X\n",
                       response.status, response.data_rx);
            }
        } else {
            printf(BRIDGE_NAME "RX pipe not ready, dropping response\n");
        }
    }
    
    printf(BRIDGE_NAME "Worker thread terminated\n");
    return NULL;
}

static int process_driver_command(const i2c_regs_t *cmd, i2c_regs_t *response) {
    static uint8_t current_data = 0;
    
    // Handle reset
    if (cmd->control & I2C_CTRL_RESET) {
        printf(BRIDGE_NAME "Reset command received\n");
        response->status = 0;
        return I2C_SUCCESS;
    }
    
    // Simulate some processing delay
    usleep(10000); // 10ms delay
    
    uint16_t slave_addr = cmd->addr & 0x7F; // 7-bit address
    bool is_read = (cmd->control & I2C_CTRL_READ) != 0;
    bool has_start = (cmd->control & I2C_CTRL_START) != 0;
    bool has_stop = (cmd->control & I2C_CTRL_STOP) != 0;
    
    printf(BRIDGE_NAME "Transaction: addr=0x%02X, %s, start=%d, stop=%d\n",
           slave_addr, is_read ? "READ" : "WRITE", has_start, has_stop);
    
    if (is_read) {
        uint8_t read_data;
        int result = simulate_i2c_transaction(slave_addr, true, 0, &read_data);
        if (result == I2C_SUCCESS) {
            response->data_rx = read_data;
            response->status = I2C_STAT_ACK | I2C_STAT_DATA_VALID;
        } else {
            response->status = I2C_STAT_ERROR;
        }
        return result;
    } else if (cmd->control & I2C_CTRL_WRITE) {
        current_data = cmd->data_tx & 0xFF;
        int result = simulate_i2c_transaction(slave_addr, false, current_data, NULL);
        if (result == I2C_SUCCESS) {
            response->status = I2C_STAT_ACK;
        } else {
            response->status = I2C_STAT_ERROR;
        }
        return result;
    }
    
    return I2C_SUCCESS;
}

static int simulate_i2c_transaction(uint16_t addr, bool is_read, uint8_t data, uint8_t *response_data) {
    i2c_slave_device_t *slave = find_slave_device(addr);
    if (!slave) {
        printf(BRIDGE_NAME "No slave device at address 0x%02X (NACK)\n", addr);
        return I2C_ERROR_NACK;
    }
    
    // Use per-slave memory offset instead of global static
    static size_t memory_offsets[MAX_SLAVES] = {0};
    static bool offsets_initialized = false;
    
    if (!offsets_initialized) {
        memset(memory_offsets, 0, sizeof(memory_offsets));
        offsets_initialized = true;
    }
    
    // Find slave index
    int slave_index = -1;
    for (int i = 0; i < g_bridge.num_slaves; i++) {
        if (g_bridge.slaves[i].addr == addr) {
            slave_index = i;
            break;
        }
    }
    
    if (slave_index < 0) {
        return I2C_ERROR_NACK;
    }
    
    size_t *memory_offset = &memory_offsets[slave_index];
    
    if (is_read) {
        if (*memory_offset < slave->memory_size) {
            *response_data = slave->memory[*memory_offset];
            printf(BRIDGE_NAME "Read from slave 0x%02X[%zu]: 0x%02X\n", addr, *memory_offset, *response_data);
            (*memory_offset)++;
        } else {
            *response_data = 0xFF; // Default read value
            printf(BRIDGE_NAME "Read beyond memory bounds, returning 0xFF\n");
        }
    } else {
        if (*memory_offset < slave->memory_size) {
            slave->memory[*memory_offset] = data;
            printf(BRIDGE_NAME "Write to slave 0x%02X[%zu]: 0x%02X\n", addr, *memory_offset, data);
            (*memory_offset)++;
        } else {
            printf(BRIDGE_NAME "Write beyond memory bounds\n");
        }
    }
    
    return I2C_SUCCESS;
}

static void add_slave_device(uint16_t addr, size_t memory_size) {
    if (g_bridge.num_slaves >= MAX_SLAVES) {
        printf(BRIDGE_NAME "Cannot add more slave devices (max %d)\n", MAX_SLAVES);
        return;
    }
    
    // Check for duplicate addresses
    for (int i = 0; i < g_bridge.num_slaves; i++) {
        if (g_bridge.slaves[i].addr == addr) {
            printf(BRIDGE_NAME "Slave device at address 0x%02X already exists\n", addr);
            return;
        }
    }
    
    i2c_slave_device_t *slave = &g_bridge.slaves[g_bridge.num_slaves];
    slave->addr = addr;
    slave->memory_size = memory_size;
    slave->memory = malloc(memory_size);
    
    if (!slave->memory) {
        printf(BRIDGE_NAME "Failed to allocate memory for slave device 0x%02X\n", addr);
        return;
    }
    
    slave->enabled = true;
    
    // Initialize memory with pattern
    for (size_t i = 0; i < memory_size; i++) {
        slave->memory[i] = (uint8_t)(i & 0xFF);
    }
    
    g_bridge.num_slaves++;
    printf(BRIDGE_NAME "Added slave device at 0x%02X with %zu bytes memory\n", addr, memory_size);
}

static i2c_slave_device_t *find_slave_device(uint16_t addr) {
    for (int i = 0; i < g_bridge.num_slaves; i++) {
        if (g_bridge.slaves[i].addr == addr && g_bridge.slaves[i].enabled) {
            return &g_bridge.slaves[i];
        }
    }
    return NULL;
}

static void cleanup_bridge(void) {
    printf(BRIDGE_NAME "Cleaning up...\n");
    
    g_bridge.running = false;
    
    // Stop worker thread
    if (g_bridge.worker_thread) {
        pthread_join(g_bridge.worker_thread, NULL);
        g_bridge.worker_thread = 0;
    }
    
    // Close pipes
    if (g_bridge.fd_tx >= 0) {
        close(g_bridge.fd_tx);
        g_bridge.fd_tx = -1;
    }
    if (g_bridge.fd_rx >= 0) {
        close(g_bridge.fd_rx);
        g_bridge.fd_rx = -1;
    }
    
    // Stop simulation
    stop_rtl_simulation();
    
    // Free slave device memory
    for (int i = 0; i < g_bridge.num_slaves; i++) {
        if (g_bridge.slaves[i].memory) {
            free(g_bridge.slaves[i].memory);
            g_bridge.slaves[i].memory = NULL;
        }
    }
    g_bridge.num_slaves = 0;
    
    // Remove pipes
    unlink("/tmp/i2c_sim_tx");
    unlink("/tmp/i2c_sim_rx");
    
    // Remove PID file
    unlink(PID_FILE);
    
    printf(BRIDGE_NAME "Cleanup complete\n");
}

// ============================================================================
// Daemon Functions
// ============================================================================

static int daemonize(void) {
    pid_t pid, sid;
    
    // Fork the first time
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    
    // Exit parent process
    if (pid > 0) {
        exit(0);
    }
    
    // Create new session
    sid = setsid();
    if (sid < 0) {
        return -1;
    }
    
    // Fork again
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    
    // Exit first child
    if (pid > 0) {
        exit(0);
    }
    
    // Change working directory to root
    if (chdir("/") < 0) {
        return -1;
    }
    
    // Set file permissions
    umask(0);
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect to log file
    int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd >= 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        if (log_fd > STDERR_FILENO) {
            close(log_fd);
        }
    }
    
    // Redirect stdin to /dev/null
    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDIN_FILENO);
        if (null_fd > STDIN_FILENO) {
            close(null_fd);
        }
    }
    
    return 0;
}

static void write_pid_file(pid_t pid) {
    FILE *fp = fopen(PID_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", pid);
        fclose(fp);
    }
}

static pid_t read_pid_file(void) {
    FILE *fp = fopen(PID_FILE, "r");
    if (fp) {
        pid_t pid;
        if (fscanf(fp, "%d", &pid) == 1) {
            fclose(fp);
            return pid;
        }
        fclose(fp);
    }
    return -1;
}
