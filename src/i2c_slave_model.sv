// ============================================================================
// I2C Slave Model
// ============================================================================
// Description: Simple I2C slave model for testing the I2C master controller
//              Features:
//              - Responds to specified slave address
//              - Simple memory array for read/write operations
//              - Configurable ACK/NACK responses
// ============================================================================

module i2c_slave_model #(
    parameter SLAVE_ADDR = 7'h50,
    parameter MEM_SIZE = 256
) (
    input  wire clk,
    input  wire rst_n,
    inout  wire scl,
    inout  wire sda,
    
    // Configuration
    input  wire       enable,
    input  wire       nack_addr,    // Force NACK on address
    input  wire       nack_data,    // Force NACK on data
    
    // Status
    output reg        addr_match,
    output reg        transaction_active,
    output reg [7:0]  last_received_data,
    output reg [7:0]  memory_pointer
);

    // ========================================================================
    // Internal Signals
    // ========================================================================
    
    reg [7:0] memory [0:MEM_SIZE-1];
    reg [7:0] shift_reg;
    reg [3:0] bit_count;
    reg       sda_out;
    reg       sda_oe;
    
    localparam [2:0] IDLE = 3'b000;
    localparam [2:0] RECEIVE_ADDR = 3'b001;
    localparam [2:0] SEND_ACK_ADDR = 3'b010;
    localparam [2:0] RECEIVE_DATA = 3'b011;
    localparam [2:0] SEND_ACK_DATA = 3'b100;
    localparam [2:0] SEND_DATA = 3'b101;
    localparam [2:0] RECEIVE_ACK = 3'b110;
    
    reg [2:0] state;
    
    reg start_detected;
    reg stop_detected;
    reg rw_bit;
    
    // ========================================================================
    // I2C Line Control
    // ========================================================================
    
    assign sda = sda_oe ? sda_out : 1'bz;
    
    // ========================================================================
    // Start/Stop Detection
    // ========================================================================
    
    always @(negedge sda) begin
        if (scl) start_detected <= 1'b1;
    end
    
    always @(posedge sda) begin
        if (scl) stop_detected <= 1'b1;
    end
    
    // ========================================================================
    // Main State Machine
    // ========================================================================
    
    always @(negedge scl or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            bit_count <= 4'd7;
            shift_reg <= 8'h00;
            sda_out <= 1'b1;
            sda_oe <= 1'b0;
            addr_match <= 1'b0;
            transaction_active <= 1'b0;
            last_received_data <= 8'h00;
            memory_pointer <= 8'h00;
            start_detected <= 1'b0;
            stop_detected <= 1'b0;
        end else begin
            
            if (stop_detected) begin
                state <= IDLE;
                transaction_active <= 1'b0;
                sda_oe <= 1'b0;
                stop_detected <= 1'b0;
            end else if (start_detected && enable) begin
                state <= RECEIVE_ADDR;
                bit_count <= 4'd7;
                shift_reg <= 8'h00;
                transaction_active <= 1'b1;
                start_detected <= 1'b0;
            end else begin
                
                case (state)
                    IDLE: begin
                        sda_oe <= 1'b0;
                        addr_match <= 1'b0;
                    end
                    
                    RECEIVE_ADDR: begin
                        shift_reg <= {shift_reg[6:0], sda};
                        if (bit_count == 4'd0) begin
                            if (shift_reg[7:1] == SLAVE_ADDR) begin
                                addr_match <= 1'b1;
                                rw_bit <= shift_reg[0];
                                state <= SEND_ACK_ADDR;
                            end else begin
                                state <= IDLE;
                                transaction_active <= 1'b0;
                            end
                        end else begin
                            bit_count <= bit_count - 1;
                        end
                    end
                    
                    SEND_ACK_ADDR: begin
                        sda_oe <= 1'b1;
                        sda_out <= nack_addr ? 1'b1 : 1'b0;  // ACK or NACK
                        bit_count <= 4'd7;
                        if (nack_addr) begin
                            state <= IDLE;
                            transaction_active <= 1'b0;
                        end else if (rw_bit) begin
                            state <= SEND_DATA;
                            shift_reg <= memory[memory_pointer];
                        end else begin
                            state <= RECEIVE_DATA;
                        end
                    end
                    
                    RECEIVE_DATA: begin
                        sda_oe <= 1'b0;
                        shift_reg <= {shift_reg[6:0], sda};
                        if (bit_count == 4'd0) begin
                            last_received_data <= {shift_reg[6:0], sda};
                            memory[memory_pointer] <= {shift_reg[6:0], sda};
                            memory_pointer <= memory_pointer + 1;
                            state <= SEND_ACK_DATA;
                        end else begin
                            bit_count <= bit_count - 1;
                        end
                    end
                    
                    SEND_ACK_DATA: begin
                        sda_oe <= 1'b1;
                        sda_out <= nack_data ? 1'b1 : 1'b0;
                        bit_count <= 4'd7;
                        state <= RECEIVE_DATA;  // Ready for next byte
                    end
                    
                    SEND_DATA: begin
                        sda_oe <= 1'b1;
                        sda_out <= shift_reg[7];
                        shift_reg <= {shift_reg[6:0], 1'b0};
                        if (bit_count == 4'd0) begin
                            state <= RECEIVE_ACK;
                            sda_oe <= 1'b0;  // Release for master ACK/NACK
                        end else begin
                            bit_count <= bit_count - 1;
                        end
                    end
                    
                    RECEIVE_ACK: begin
                        bit_count <= 4'd7;
                        if (sda) begin  // NACK received
                            state <= IDLE;
                            transaction_active <= 1'b0;
                        end else begin  // ACK received
                            memory_pointer <= memory_pointer + 1;
                            shift_reg <= memory[memory_pointer + 1];
                            state <= SEND_DATA;
                        end
                    end
                    
                    default: state <= IDLE;
                endcase
            end
        end
    end
    
    // ========================================================================
    // Memory Initialization
    // ========================================================================
    
    integer i;
    initial begin
        for (i = 0; i < MEM_SIZE; i = i + 1) begin
            memory[i] = i[7:0];  // Initialize with pattern
        end
    end
    
endmodule
