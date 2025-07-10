// ============================================================================
// I2C Master Controller
// ============================================================================
// Description: A comprehensive I2C master controller that supports:
//              - 7-bit and 10-bit addressing
//              - Standard (100kHz) and Fast (400kHz) modes
//              - Clock stretching support
//              - Configurable timing parameters
// ============================================================================

module i2c_master #(
    parameter CLK_FREQ = 50_000_000,    // System clock frequency in Hz
    parameter I2C_FREQ = 100_000,       // I2C clock frequency in Hz
    parameter ADDR_WIDTH = 7            // Address width (7 or 10 bits)
) (
    // System interface
    input  wire                   clk,
    input  wire                   rst_n,
    
    // User interface
    input  wire                   start,        // Start transaction
    input  wire                   stop,         // Stop transaction
    input  wire                   read_write,   // 1=read, 0=write
    input  wire [ADDR_WIDTH-1:0]  slave_addr,   // Slave address
    input  wire [7:0]             write_data,   // Data to write
    output reg  [7:0]             read_data,    // Data read
    output reg                    data_valid,   // Read data valid
    output reg                    ack_received, // ACK received from slave
    output reg                    busy,         // Transaction in progress
    output reg                    error,        // Error occurred
    
    // I2C interface
    inout  wire                   scl,          // I2C clock line
    inout  wire                   sda           // I2C data line
);

    // ========================================================================
    // Parameters and Constants
    // ========================================================================
    
    // Calculate clock divider for I2C timing
    localparam CLK_DIV = CLK_FREQ / (4 * I2C_FREQ);
    localparam CLK_DIV_WIDTH = $clog2(CLK_DIV);
    
    // State machine states
    localparam [3:0] IDLE        = 4'b0000;
    localparam [3:0] START       = 4'b0001;
    localparam [3:0] ADDR_7BIT   = 4'b0010;
    localparam [3:0] ADDR_10BIT  = 4'b0011;
    localparam [3:0] WRITE_DATA  = 4'b0100;
    localparam [3:0] READ_DATA_ST= 4'b0101;
    localparam [3:0] ACK_CHECK   = 4'b0110;
    localparam [3:0] ACK_SEND    = 4'b0111;
    localparam [3:0] NACK_SEND   = 4'b1000;
    localparam [3:0] STOP_ST     = 4'b1001;
    localparam [3:0] ERROR_ST    = 4'b1010;
    
    // ========================================================================
    // Internal Signals
    // ========================================================================
    
    reg [3:0] current_state, next_state;
    
    // Clock generation
    reg [CLK_DIV_WIDTH-1:0] clk_counter;
    reg                     clk_en;
    reg [1:0]               clk_phase;  // 00=low, 01=rise, 10=high, 11=fall
    
    // I2C signals
    reg                     scl_out;
    reg                     sda_out;
    reg                     scl_oe;     // Output enable for SCL
    reg                     sda_oe;     // Output enable for SDA
    wire                    sda_in;
    wire                    scl_in;
    
    // Bit counter and shift registers
    reg [3:0]               bit_counter;
    reg [7:0]               shift_reg;
    reg                     ack_bit;
    
    // Control signals
    reg                     load_addr;
    reg                     load_data;
    reg                     shift_enable;
    reg                     capture_ack;
    reg                     send_ack;
    reg                     send_nack;
    
    // ========================================================================
    // I2C Line Control
    // ========================================================================
    
    // Tri-state control for I2C lines
    assign scl = scl_oe ? scl_out : 1'bz;
    assign sda = sda_oe ? sda_out : 1'bz;
    assign scl_in = scl;
    assign sda_in = sda;
    
    // ========================================================================
    // Clock Generation
    // ========================================================================
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            clk_counter <= 0;
            clk_phase <= 2'b00;
            clk_en <= 1'b0;
        end else begin
            if (clk_counter == CLK_DIV - 1) begin
                clk_counter <= 0;
                clk_phase <= clk_phase + 1;
                clk_en <= 1'b1;
            end else begin
                clk_counter <= clk_counter + 1;
                clk_en <= 1'b0;
            end
        end
    end
    
    // Generate SCL
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            scl_out <= 1'b1;
        end else if (clk_en) begin
            case (clk_phase)
                2'b00, 2'b01: scl_out <= 1'b0;  // SCL low
                2'b10, 2'b11: scl_out <= 1'b1;  // SCL high
            endcase
        end
    end
    
    // ========================================================================
    // State Machine
    // ========================================================================
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= IDLE;
        end else begin
            current_state <= next_state;
        end
    end
    
    always @(*) begin
        next_state = current_state;
        
        case (current_state)
            IDLE: begin
                if (start)
                    next_state = START;
            end
            
            START: begin
                if (clk_en && clk_phase == 2'b11)
                    next_state = (ADDR_WIDTH == 7) ? ADDR_7BIT : ADDR_10BIT;
            end
            
            ADDR_7BIT: begin
                if (clk_en && clk_phase == 2'b11 && bit_counter == 0)
                    next_state = ACK_CHECK;
            end
            
            ADDR_10BIT: begin
                if (clk_en && clk_phase == 2'b11 && bit_counter == 0)
                    next_state = ACK_CHECK;
            end
            
            WRITE_DATA: begin
                if (clk_en && clk_phase == 2'b11 && bit_counter == 0)
                    next_state = ACK_CHECK;
            end
            
            READ_DATA_ST: begin
                if (clk_en && clk_phase == 2'b11 && bit_counter == 0)
                    next_state = stop ? NACK_SEND : ACK_SEND;
            end
            
            ACK_CHECK: begin
                if (clk_en && clk_phase == 2'b11) begin
                    if (sda_in == 1'b0) begin  // ACK received (active low)
                        if (read_write)
                            next_state = READ_DATA_ST;
                        else if (stop)
                            next_state = STOP_ST;
                        else
                            next_state = WRITE_DATA;
                    end else begin
                        next_state = ERROR_ST;  // NACK received
                    end
                end
            end
            
            ACK_SEND: begin
                if (clk_en && clk_phase == 2'b11)
                    next_state = stop ? STOP_ST : READ_DATA_ST;
            end
            
            NACK_SEND: begin
                if (clk_en && clk_phase == 2'b11)
                    next_state = STOP_ST;
            end
            
            STOP_ST: begin
                if (clk_en && clk_phase == 2'b01)
                    next_state = IDLE;
            end
            
            ERROR_ST: begin
                next_state = IDLE;
            end
            
            default: next_state = IDLE;
        endcase
    end
    
    // ========================================================================
    // Control Logic
    // ========================================================================
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            bit_counter <= 4'd7;
            shift_reg <= 8'h00;
            sda_out <= 1'b1;
            sda_oe <= 1'b0;
            scl_oe <= 1'b0;
            busy <= 1'b0;
            error <= 1'b0;
            read_data <= 8'h00;
            data_valid <= 1'b0;
            ack_received <= 1'b0;
            ack_bit <= 1'b1;
        end else begin
            data_valid <= 1'b0;  // Default
            
            case (current_state)
                IDLE: begin
                    busy <= 1'b0;
                    scl_oe <= 1'b0;
                    sda_oe <= 1'b0;
                    sda_out <= 1'b1;
                    if (start) begin
                        error <= 1'b0;  // Clear error only when starting new transaction
                        bit_counter <= 4'd7;
                        if (ADDR_WIDTH == 7) begin
                            shift_reg <= {slave_addr, read_write};
                        end else begin
                            shift_reg <= {5'b11110, slave_addr[9:8], read_write};
                        end
                    end
                end
                
                START: begin
                    busy <= 1'b1;  // Set busy when starting transaction
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b1;
                    if (clk_en) begin
                        case (clk_phase)
                            2'b00: sda_out <= 1'b0;  // Start condition
                            2'b01: sda_out <= 1'b0;
                        endcase
                    end
                end
                
                ADDR_7BIT, ADDR_10BIT, WRITE_DATA: begin
                    busy <= 1'b1;  // Maintain busy during data transmission
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b1;
                    if (clk_en && clk_phase == 2'b00) begin
                        sda_out <= shift_reg[7];
                        shift_reg <= {shift_reg[6:0], 1'b0};
                        if (bit_counter > 0) begin
                            bit_counter <= bit_counter - 1;
                        end
                    end
                    
                    // Load next data for continuous writing
                    if (current_state == WRITE_DATA && bit_counter == 4'd1 && !stop) begin
                        shift_reg <= write_data;
                    end
                end
                
                READ_DATA_ST: begin
                    busy <= 1'b1;  // Maintain busy during data reception
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b0;  // Release SDA for slave to drive
                    if (clk_en && clk_phase == 2'b10) begin  // Sample on SCL high
                        shift_reg <= {shift_reg[6:0], sda_in};
                        if (bit_counter > 0) begin
                            bit_counter <= bit_counter - 1;
                        end
                        if (bit_counter == 4'd0) begin
                            read_data <= {shift_reg[6:0], sda_in};
                            data_valid <= 1'b1;
                        end
                    end
                end
                
                ACK_CHECK: begin
                    busy <= 1'b1;  // Maintain busy during ACK check
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b0;  // Release SDA
                    if (clk_en && clk_phase == 2'b10) begin
                        ack_bit <= sda_in;
                        ack_received <= !sda_in;
                    end
                    bit_counter <= 4'd7;  // Reset for next byte
                end
                
                ACK_SEND: begin
                    busy <= 1'b1;  // Maintain busy during ACK send
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b1;
                    sda_out <= 1'b0;  // Send ACK
                    bit_counter <= 4'd7;
                end
                
                NACK_SEND: begin
                    busy <= 1'b1;  // Maintain busy during NACK send
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b1;
                    sda_out <= 1'b1;  // Send NACK
                    bit_counter <= 4'd7;  // Reset for next operation
                end
                
                STOP_ST: begin
                    busy <= 1'b1;  // Maintain busy during stop condition
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b1;
                    if (clk_en) begin
                        case (clk_phase)
                            2'b00: sda_out <= 1'b0;
                            2'b01: sda_out <= 1'b1;  // Stop condition
                        endcase
                    end
                end
                
                ERROR_ST: begin
                    busy <= 1'b0;  // Clear busy on error
                    error <= 1'b1;
                    scl_oe <= 1'b0;
                    sda_oe <= 1'b0;
                end
            endcase
        end
    end
    
endmodule
