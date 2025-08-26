// rv_cosim_tb.sv
// Questa/ModelSim file-based harness for adder_rv_simple
// - Reads hex pairs from +cosim_inputs=inputs.txt
// - Drives ready/valid into DUT (holds in_valid until in_ready @ posedge)
// - Keeps out_ready=1 and writes sums to +cosim_outputs=outputs.txt (one 8-hex per line)
`timescale 1ns / 1ps

module rv_cosim_tb;
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

  // clock 100MHz
  always #5 clk = ~clk;

  // files
  string in_path  = "inputs.txt";
  string out_path = "outputs.txt";
  integer fin, fout;

  // counters
  int n_sent = 0;
  int n_written = 0;

  // consumer: write sum when transfer happens
  always @(posedge clk) begin
    if (out_valid && out_ready) begin
      $fwrite(fout, "%08h\n", out_sum);
      n_written++;
    end
  end

  task automatic send_item(input logic [W-1:0] a, input logic [W-1:0] b);
    // Drive pre-edge
    @(negedge clk);
    in_a     <= a;
    in_b     <= b;
    in_valid <= 1'b1;
    // Wait for acceptance on a posedge
    do @(posedge clk); while (!in_ready);
    // Deassert on next negedge
    @(negedge clk);
    in_valid <= 1'b0;
  endtask

  initial begin
    // defaults
    in_valid = 1'b0;
    in_a = '0; in_b = '0;
    out_ready = 1'b1;

    void'($value$plusargs("cosim_inputs=%s",  in_path));
    void'($value$plusargs("cosim_outputs=%s", out_path));

    // reset
    repeat (4) @(posedge clk);
    rst_n = 1'b1;
    @(posedge clk);

    // open files
    fin  = $fopen(in_path, "r");
    if (fin == 0) $fatal(1, "[TB] Cannot open input %s", in_path);
    fout = $fopen(out_path, "w");
    if (fout == 0) $fatal(1, "[TB] Cannot open output %s", out_path);

    string line;
    logic [W-1:0] a, b;
    int r;
    while (!$feof(fin)) begin
      r = $fscanf(fin, "%h %h\n", a, b);
      if (r == 2) begin
        send_item(a, b);
        n_sent++;
      end else begin
        // Reads a line of text from the file handle fin into the string variable line,
        // advancing the file position to just after the newline (end-of-line).
        // If $fscanf failed or only partially matched, $fgets consumes the remainder
        // of that line so the next read starts on the next line.
        // --> Useful to discard blank/comment/garbage lines or to skip a partially parsed line.
        // void' means we are ignoring the return value.
        void'($fgets(line, fin));
      end
    end
    $fclose(fin);

    // wait for all outputs
    wait (n_written == n_sent);
    @(posedge clk);

    $display("[TB] DONE sent=%0d written=%0d -> %s", n_sent, n_written, out_path);
    $fclose(fout);
    $finish;
  end
endmodule
