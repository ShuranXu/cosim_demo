// adder_cosim_tb_fixed.sv
// Questa/ModelSim file-based harness for adder_rv_simple (Linux-friendly)
// - Reads hex pairs from a text file
// - Drives ready/valid into DUT (producer holds until in_ready on posedge)
// - Keeps out_ready=1 and writes sums to an output text file
// Supports compile-time macros for file paths; optional plusargs can override.
//
// Macros (string literals) you can pass via vlog +define:
//   COSIM_INPUTS  -> default "inputs.txt"
//   COSIM_OUTPUTS -> default "outputs.txt"
`timescale 1ns/1ps

`define STR_IMPL(x) `"x`"
`define STR(x) `STR_IMPL(x)

`ifndef COSIM_INPUTS
  `define COSIM_INPUTS "inputs.txt"
`endif
`ifndef COSIM_OUTPUTS
  `define COSIM_OUTPUTS "outputs.txt"
`endif

module adder_cosim_tb;
  parameter int W = 32;

  // Clock/reset
  logic clk = 0;
  logic rst_n = 0;

  // Producer -> DUT
  logic         in_valid;
  wire          in_ready;
  logic [W-1:0] in_a;
  logic [W-1:0] in_b;

  // DUT -> Consumer
  wire          out_valid;
  logic         out_ready;
  wire  [W-1:0] out_sum;

  // DUT instance
  adder_rv_simple #(.W(W)) dut (
    .clk        (clk),
    .rst_n      (rst_n),
    .in_valid   (in_valid),
    .in_ready   (in_ready),
    .in_a       (in_a),
    .in_b       (in_b),
    .out_valid  (out_valid),
    .out_ready  (out_ready),
    .out_sum    (out_sum)
  );

  // 100 MHz clock
  always #5 clk = ~clk;

  // File paths (from macros, with optional plusargs override)
  string in_path  = `STR(`COSIM_INPUTS);
  string out_path = `STR(`COSIM_OUTPUTS);

  // I/O handles and counters
  integer fin, fout;
  int n_sent = 0;
  int n_written = 0;

  string line;
  logic [W-1:0] a, b;
  int r;

  // Consumer: write when a transfer completes (respect reset)
  always @(posedge clk) begin
    if (rst_n && out_valid && out_ready) begin
      $fwrite(fout, "%08h\n", out_sum);
      n_written++;
    end
  end

  // Producer: send one item, holding until accepted at a posedge
  task automatic send_item(input logic [W-1:0] a, input logic [W-1:0] b);
    @(negedge clk);
    in_a     <= a;
    in_b     <= b;
    in_valid <= 1'b1;
    // Wait for acceptance on a posedge
    do @(posedge clk); while (!in_ready);
    @(negedge clk);
    in_valid <= 1'b0;
  endtask

  // Main control (single initial block; all statements kept inside)
  initial begin
    // Defaults
    in_valid  = 1'b0;
    in_a      = '0;
    in_b      = '0;
    out_ready = 1'b1;

    // Optional plusargs to override file paths
    void'($value$plusargs("cosim_inputs=%s",  in_path));
    void'($value$plusargs("cosim_outputs=%s", out_path));

    // Reset sequence
    repeat (4) @(posedge clk);
    rst_n = 1'b1;
    @(posedge clk);

    // Open files
    fin  = $fopen(in_path,  "r");
    if (fin == 0) $fatal(1, "[TB] Cannot open input %s", in_path);
    fout = $fopen(out_path, "w");
    if (fout == 0) $fatal(1, "[TB] Cannot open output %s", out_path);

    // Read lines and drive
    while (!$feof(fin)) begin
      r = $fscanf(fin, "%h %h\n", a, b);
      if (r == 2) begin
        send_item(a, b);
        n_sent++;
      end else begin
        // consume the rest of the line (skip blanks/comments/garbage)
        void'($fgets(line, fin));
      end
    end
    $fclose(fin);

    // Drain: wait until all outputs have been written
    wait (n_written == n_sent);
    @(posedge clk);

    $display("[TB] DONE sent=%0d written=%0d -> %s", n_sent, n_written, out_path);
    $fclose(fout);
    $finish;
  end

endmodule
