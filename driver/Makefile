# ============================================================================
# I2C Driver Test Suite Makefile
# ============================================================================
# Description: Build system for I2C driver, bridge, and test programs
# ============================================================================

# Compiler settings
CC = clang
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lpthread

# Directories
INCLUDE_DIR = include
TEST_DIR = test
BUILD_DIR = build

# Source files
DRIVER_SOURCES = i2c_driver.c
BRIDGE_SOURCES = i2c_bridge.c
TEST_SOURCES = $(TEST_DIR)/test_i2c_driver.c
INTERACTIVE_SOURCES = $(TEST_DIR)/interactive_test.c
SIMPLE_SOURCES = $(TEST_DIR)/simple_test.c

# Target executables
DRIVER_LIB = $(BUILD_DIR)/libi2c_driver.a
BRIDGE_BIN = $(BUILD_DIR)/i2c_bridge
TEST_BIN = $(BUILD_DIR)/test_i2c_driver
INTERACTIVE_BIN = $(BUILD_DIR)/interactive_test
SIMPLE_BIN = $(BUILD_DIR)/simple_test

# Object files
DRIVER_OBJS = $(BUILD_DIR)/i2c_driver.o
BRIDGE_OBJS = $(BUILD_DIR)/i2c_bridge.o
TEST_OBJS = $(BUILD_DIR)/test_i2c_driver.o
INTERACTIVE_OBJS = $(BUILD_DIR)/interactive_test.o
SIMPLE_OBJS = $(BUILD_DIR)/simple_test.o

# Default target
all: setup $(DRIVER_LIB) $(BRIDGE_BIN) $(TEST_BIN) $(INTERACTIVE_BIN) $(SIMPLE_BIN)

# Create build directory
setup:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p scripts

# Driver library
$(DRIVER_LIB): $(DRIVER_OBJS)
	@echo "Creating driver library..."
	ar rcs $@ $^

$(BUILD_DIR)/i2c_driver.o: $(DRIVER_SOURCES) $(INCLUDE_DIR)/i2c_driver.h
	@echo "Compiling I2C driver..."
	$(CC) $(CFLAGS) -I. -c $(DRIVER_SOURCES) -o $@

# Bridge executable
$(BRIDGE_BIN): $(BRIDGE_OBJS) $(DRIVER_LIB)
	@echo "Linking I2C bridge..."
	$(CC) $(CFLAGS) -I. $< $(DRIVER_LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/i2c_bridge.o: $(BRIDGE_SOURCES) $(INCLUDE_DIR)/i2c_driver.h
	@echo "Compiling I2C bridge..."
	$(CC) $(CFLAGS) -I. -c $(BRIDGE_SOURCES) -o $@

# Test executable
$(TEST_BIN): $(TEST_OBJS) $(DRIVER_LIB)
	@echo "Linking test suite..."
	$(CC) $(CFLAGS) -I. $< $(DRIVER_LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/test_i2c_driver.o: $(TEST_SOURCES) $(INCLUDE_DIR)/i2c_driver.h
	@echo "Compiling test suite..."
	$(CC) $(CFLAGS) -I. -c $(TEST_SOURCES) -o $@

# Interactive test executable
$(INTERACTIVE_BIN): $(INTERACTIVE_OBJS) $(DRIVER_LIB)
	@echo "Linking interactive test..."
	$(CC) $(CFLAGS) -I. $< $(DRIVER_LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/interactive_test.o: $(INTERACTIVE_SOURCES) $(INCLUDE_DIR)/i2c_driver.h
	@echo "Compiling interactive test..."
	$(CC) $(CFLAGS) -I. -c $(INTERACTIVE_SOURCES) -o $@

# Simple test executable
$(SIMPLE_BIN): $(SIMPLE_OBJS) $(DRIVER_LIB)
	@echo "Linking simple test..."
	$(CC) $(CFLAGS) -I. $< $(DRIVER_LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/simple_test.o: $(SIMPLE_SOURCES) $(INCLUDE_DIR)/i2c_driver.h
	@echo "Compiling simple test..."
	$(CC) $(CFLAGS) -I. -c $(SIMPLE_SOURCES) -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f /tmp/i2c_sim_tx /tmp/i2c_sim_rx

# Build RTL simulation
rtl:
	@echo "Building RTL simulation..."
	$(MAKE) -C . simulate

# Start I2C bridge as daemon
start-bridge: $(BRIDGE_BIN)
	@echo "Starting I2C bridge daemon..."
	@if [ -f /tmp/i2c_bridge.pid ]; then \
		PID=$$(cat /tmp/i2c_bridge.pid 2>/dev/null); \
		if [ -n "$$PID" ] && kill -0 $$PID 2>/dev/null; then \
			echo "Bridge is already running (PID: $$PID)"; \
			exit 1; \
		else \
			echo "Removing stale PID file"; \
			rm -f /tmp/i2c_bridge.pid; \
		fi; \
	fi
	@$(BRIDGE_BIN) -d
	@sleep 1
	@if [ -f /tmp/i2c_bridge.pid ]; then \
		PID=$$(cat /tmp/i2c_bridge.pid 2>/dev/null); \
		if [ -n "$$PID" ] && kill -0 $$PID 2>/dev/null; then \
			echo "Bridge started successfully (PID: $$PID)"; \
			echo "Logs are written to /tmp/i2c_bridge.log"; \
		else \
			echo "Failed to start bridge daemon"; \
			exit 1; \
		fi; \
	else \
		echo "Failed to start bridge daemon (no PID file)"; \
		exit 1; \
	fi

# Stop I2C bridge daemon
stop-bridge:
	@echo "Stopping I2C bridge daemon..."
	@if [ -f /tmp/i2c_bridge.pid ]; then \
		PID=$$(cat /tmp/i2c_bridge.pid 2>/dev/null); \
		if [ -n "$$PID" ]; then \
			if kill -0 $$PID 2>/dev/null; then \
				kill -TERM $$PID; \
				echo "Sent SIGTERM to bridge (PID: $$PID)"; \
				for i in 1 2 3 4 5; do \
					if ! kill -0 $$PID 2>/dev/null; then \
						echo "Bridge stopped successfully"; \
						rm -f /tmp/i2c_bridge.pid; \
						exit 0; \
					fi; \
					sleep 1; \
				done; \
				echo "Bridge did not stop gracefully, forcing..."; \
				kill -KILL $$PID 2>/dev/null; \
				rm -f /tmp/i2c_bridge.pid; \
				echo "Bridge force-stopped"; \
			else \
				echo "Bridge PID $$PID is not running"; \
				rm -f /tmp/i2c_bridge.pid; \
			fi; \
		else \
			echo "Invalid PID file"; \
			rm -f /tmp/i2c_bridge.pid; \
		fi; \
	else \
		echo "Bridge is not running (no PID file)"; \
	fi

# Check bridge status
status-bridge:
	@if [ -f /tmp/i2c_bridge.pid ]; then \
		PID=$$(cat /tmp/i2c_bridge.pid 2>/dev/null); \
		if [ -n "$$PID" ] && kill -0 $$PID 2>/dev/null; then \
			echo "Bridge is running (PID: $$PID)"; \
			echo "Log file: /tmp/i2c_bridge.log"; \
			if [ -f /tmp/i2c_bridge.log ]; then \
				echo "Log size: $$(wc -c < /tmp/i2c_bridge.log) bytes"; \
			fi; \
			echo "Named pipes:"; \
			ls -l /tmp/i2c_sim_* 2>/dev/null || echo "  No pipes found"; \
		else \
			echo "Bridge is not running (stale PID file)"; \
			rm -f /tmp/i2c_bridge.pid; \
		fi; \
	else \
		echo "Bridge is not running"; \
	fi

# Restart bridge (stop + start)
restart-bridge: stop-bridge start-bridge
	@echo "Bridge restarted"

# Show bridge logs
logs-bridge:
	@if [ -f /tmp/i2c_bridge.log ]; then \
		echo "=== I2C Bridge Logs ==="; \
		tail -50 /tmp/i2c_bridge.log; \
	else \
		echo "No log file found"; \
	fi

# Run test suite with optional DEBUG flag
test: $(TEST_BIN)
	@echo "Running I2C driver test suite..."
	@if [ "$(DEBUG)" = "1" ]; then \
		echo "Rebuilding with debug symbols..."; \
		$(MAKE) clean; \
		$(MAKE) CFLAGS="-Wall -Wextra -std=c99 -g" all; \
		echo "Running test suite in lldb..."; \
		lldb -- $(TEST_BIN); \
	else \
		$(TEST_BIN); \
	fi

# Run simple test
simple: $(SIMPLE_BIN)
	@echo "Running simple I2C test..."
	$(SIMPLE_BIN)

# Run interactive test
interactive: $(INTERACTIVE_BIN)
	@echo "Starting interactive I2C test..."
	@echo "Make sure the bridge is running first with 'make start-bridge'"
	$(INTERACTIVE_BIN)

# Full test cycle
full-test: clean all start-bridge
	@echo "Waiting for bridge to initialize..."
	@sleep 3
	@echo "Running test suite..."
	$(TEST_BIN) || true
	@echo "Stopping bridge..."
	$(MAKE) stop-bridge

# Install to system (optional)
install: all
	@echo "Installing I2C driver tools..."
	sudo cp $(BRIDGE_BIN) /usr/local/bin/
	sudo cp $(TEST_BIN) /usr/local/bin/
	sudo cp $(INTERACTIVE_BIN) /usr/local/bin/
	sudo cp $(INCLUDE_DIR)/i2c_driver.h /usr/local/include/
	sudo cp $(DRIVER_LIB) /usr/local/lib/

# Uninstall from system
uninstall:
	@echo "Uninstalling I2C driver tools..."
	sudo rm -f /usr/local/bin/i2c_bridge
	sudo rm -f /usr/local/bin/test_i2c_driver
	sudo rm -f /usr/local/bin/interactive_test
	sudo rm -f /usr/local/include/i2c_driver.h
	sudo rm -f /usr/local/lib/libi2c_driver.a

help:
	@echo "I2C Driver Test Suite Makefile"
	@echo "================================"
	@echo ""
	@echo "Build Targets:"
	@echo "  all            - Build all components"
	@echo "  clean          - Clean build artifacts"
	@echo ""
	@echo "Bridge Service Targets:"
	@echo "  start-bridge   - Start I2C bridge daemon"
	@echo "  stop-bridge    - Stop I2C bridge daemon"
	@echo "  status-bridge  - Check bridge status"
	@echo "  restart-bridge - Restart bridge daemon"
	@echo "  logs-bridge    - Show recent bridge logs"
	@echo ""
	@echo "Test Targets:"
	@echo "  simple         - Run simple test"
	@echo "  test           - Run automated test suite"
	@echo "  interactive    - Run interactive test tool"
	@echo "  full-test      - Complete test cycle (build, start, test, stop)"
	@echo ""
	@echo "System Targets:"
	@echo "  install        - Install to system"
	@echo "  uninstall      - Remove from system"
	@echo "  help           - Show this help"
	@echo ""
	@echo "Quick Start:"
	@echo "  make all                 # Build everything"
	@echo "  make start-bridge        # Start bridge daemon"
	@echo "  make status-bridge       # Check if running"
	@echo "  make simple              # Run simple test"
	@echo "  make logs-bridge         # Check logs"
	@echo "  make stop-bridge         # Stop bridge when done"
	@echo ""
	@echo "Service Usage:"
	@echo "  The bridge now runs as a daemon and logs to /tmp/i2c_bridge.log"
	@echo "  Use status-bridge to check if it's running properly"

# Debug targets
debug-driver: $(DRIVER_OBJS)
	@echo "Driver object files:"
	@file $(DRIVER_OBJS)

debug-build:
	@echo "Build directory contents:"
	@ls -la $(BUILD_DIR) 2>/dev/null || echo "Build directory not found"

# Phony targets
.PHONY: all clean setup rtl start-bridge stop-bridge status-bridge restart-bridge logs-bridge test interactive full-test install uninstall help debug-driver debug-build
