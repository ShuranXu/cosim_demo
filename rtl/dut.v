`timescale 1ns/1ps

module dut #(
    parameter int DATAW = 16
)(
    input  wire                 clk,
    input  wire                 rst_n,

    // Input stream
    input  wire                 in_valid,
    output wire                 in_ready,
    input  wire signed [DATAW-1:0] in_data,

    // Output stream
    output reg                  out_valid,
    input  wire                 out_ready,
    output reg signed [2*DATAW-1:0] out_data
);
    // Simple ready/valid: always ready to accept when not in reset
    assign in_ready = rst_n;

    // Single-cycle pipeline: capture when in_valid && in_ready
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            out_valid <= 1'b0;
            out_data  <= '0;
        end else begin
            // consume new input
            if (in_valid && in_ready) begin
                out_data  <= in_data * in_data; // example transform
                out_valid <= 1'b1;
            end else if (out_valid && out_ready) begin
                out_valid <= 1'b0;
            end
        end
    end
endmodule
