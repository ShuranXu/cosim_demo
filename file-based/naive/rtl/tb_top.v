`timescale 1ns/1ps

module tb_top;
    localparam int DATAW = 16;

    // Clock/Reset
    reg clk = 0;
    reg rst_n = 0;
    always #5 clk = ~clk; // 100 MHz

    initial begin
        rst_n = 0;
        repeat (5) @(posedge clk);
        rst_n = 1;
    end

    // Stream signals
    reg                   in_valid;
    wire                  in_ready;
    reg  signed [DATAW-1:0] in_data;

    wire                  out_valid;
    reg                   out_ready;
    wire signed [2*DATAW-1:0] out_data;

    // DUT instance
    dut #(.DATAW(DATAW)) u_dut (

        .clk        (clk        ),
        .rst_n      (rst_n      ),
        .in_valid   (in_valid   ),
        .in_ready   (in_ready   ),
        .in_data    (in_data    ),
        .out_valid  (out_valid  ),
        .out_ready  (out_ready  ),
        .out_data   (out_data   )
    );

    // File paths (defaults, can override with +IN=... +OUT=...)
    string in_path  = "sim/in.dat";
    string out_path = "sim/out.dat";

    // File handles and counters
    integer fin, fout;
    integer code;
    integer sent_cnt, recv_cnt;
    integer sample;

    // Ready policy: always ready to accept DUT output
    initial out_ready = 1'b1;

    // Open files after reset deassertion
    initial begin
        // Allow overrides
        void'($value$plusargs("IN=%s",  in_path));
        void'($value$plusargs("OUT=%s", out_path));

        // Wait for reset release
        @(posedge rst_n);

        // Open input
        fin = $fopen(in_path, "r");
        if (fin == 0) begin
            $display("[%0t] TB ERROR: cannot open input file %s", $time, in_path);
            $finish;
        end

        // Open output
        fout = $fopen(out_path, "w");
        if (fout == 0) begin
            $display("[%0t] TB ERROR: cannot open output file %s", $time, out_path);
            $finish;
        end

        $display("[%0t] TB: Using IN=%s  OUT=%s", $time, in_path, out_path);
    end

    // Driver: feed one value when DUT is ready
    typedef enum int {DR_IDLE, DR_HAVE} dr_state_e;
    dr_state_e dr_st;

    initial begin
        in_valid = 1'b0;
        in_data  = '0;
        dr_st    = DR_IDLE;
        sent_cnt = 0;

        // Wait for files & reset
        @(posedge rst_n);

        forever begin
            @(posedge clk);

            case (dr_st)
                DR_IDLE: begin
                    if (! $feof(fin)) begin
                        // read an integer from the input file
                        code = $fscanf(fin, "%d\n", sample);
                        if (code == 1) begin
                            in_data  <= sample;
                            in_valid <= 1'b1;
                            dr_st    <= DR_HAVE;
                        end
                    end else begin
                        // No more input: deassert valid forever
                        in_valid <= 1'b0;
                    end
                end

                DR_HAVE: begin
                    if (in_valid && in_ready) begin
                        sent_cnt <= sent_cnt + 1;
                        in_valid <= 1'b0;
                        dr_st    <= DR_IDLE;
                    end
                end
            endcase
        end
    end

    // Monitor: capture DUT output and write to file
    initial begin
        recv_cnt = 0;
        @(posedge rst_n);
        forever begin
            @(posedge clk);
            if (out_valid && out_ready) begin
                $fdisplay(fout, "%0d", out_data);
                recv_cnt <= recv_cnt + 1;
            end
        end
    end

    // Finish condition guard
    initial begin : finish_guard
        time timeout_ns;
        time start_time;
        bit  inputs_done;

        timeout_ns = 10_000_000; // 10 ms safety
        start_time = 0;

        @(posedge rst_n);
        start_time = $time;

        forever begin
            @(posedge clk);

            // check if the input file has been read completely
            inputs_done = $feof(fin) && !in_valid;

            if (inputs_done && (recv_cnt == sent_cnt) && (sent_cnt != 0)) begin
                $display("[%0t] TB: DONE  sent=%0d  received=%0d", $time, sent_cnt, recv_cnt);
                $fclose(fin);
                $fclose(fout);
                $finish;
            end

            if (($time - start_time) >= timeout_ns) begin
                $display("[%0t] TB TIMEOUT: sent=%0d received=%0d feof=%0d in_valid=%0b",
                        $time, sent_cnt, recv_cnt, $feof(fin), in_valid);
                $fclose(fin);
                $fclose(fout);
                $finish;
            end
        end
    end

endmodule
