# Copilot Instructions for RTL I2C Project

<!-- Use this file to provide workspace-specific custom instructions to Copilot. For more details, visit https://code.visualstudio.com/docs/copilot/copilot-customization#_use-a-githubcopilotinstructionsmd-file -->

This is an RTL (Register Transfer Level) development project for designing an I2C controller module in Verilog.

## Project Guidelines

- Use SystemVerilog/Verilog HDL syntax for all RTL code
- Follow industry-standard RTL coding practices
- Use non-blocking assignments (<=) for sequential logic
- Use blocking assignments (=) for combinational logic
- Include proper reset logic for all registers
- Use meaningful signal and module names
- Add comprehensive comments for complex logic
- Include parameter definitions for configurable values
- Follow clock domain crossing best practices
- Write comprehensive testbenches with assertions
- Use proper file naming conventions (.v for Verilog, .sv for SystemVerilog)

## I2C Specific Guidelines

- Implement proper I2C timing requirements
- Handle clock stretching if needed
- Include proper start/stop condition detection
- Implement ACK/NACK handling
- Support both 7-bit and 10-bit addressing modes
- Include proper error handling and status reporting
- Follow I2C protocol specifications strictly
- Include debug and monitoring signals

## Testing Guidelines

- Create modular testbenches
- Use proper clock and reset generation
- Include comprehensive test scenarios
- Test corner cases and error conditions
- Use assertions for protocol compliance checking
- Include waveform generation for debugging
