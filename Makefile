# ============================================================================
# I2C Master Controller Makefile
# ============================================================================

# Simulation tools
IVERILOG = iverilog
VVP = vvp
GTKWAVE = gtkwave

# Directories
SRC_DIR = src
TB_DIR = testbench
SIM_DIR = sim

# Source files
SRC_FILES = $(SRC_DIR)/i2c_master.sv $(SRC_DIR)/i2c_slave_model.sv
TB_FILE = $(TB_DIR)/tb_i2c_master.sv

# Simulation target
SIM_TARGET = $(SIM_DIR)/tb_i2c_master
VCD_FILE = $(SIM_DIR)/i2c_master.vcd

# Create simulation directory
$(SIM_DIR):
	mkdir -p $(SIM_DIR)

# Compile testbench
$(SIM_TARGET): $(SRC_FILES) $(TB_FILE) | $(SIM_DIR)
	$(IVERILOG) -g2005-sv -o $@ -I$(SRC_DIR) $(TB_FILE) $(SRC_FILES)

# Run simulation
simulate: $(SIM_TARGET)
	cd $(SIM_DIR) && ./tb_i2c_master

# View waveforms
wave: $(VCD_FILE)
	$(GTKWAVE) $(VCD_FILE)

# Clean build artifacts
clean:
	rm -rf $(SIM_DIR)

# Help
help:
	@echo "Available targets:"
	@echo "  simulate - Compile and run the I2C master testbench"
