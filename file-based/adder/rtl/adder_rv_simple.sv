//==============================================================================
// Architecture diagram — 2-entry elastic adder (ready/valid)
//==============================================================================
//
//              ┌────────────────────────────────────────────────────┐
// in_a,in_b ─▶│                SUM COMPUTE (a+b)                   │
// in_valid ──▶│  (combinational add performed only when accepted)  │
//              └───────────────┬────────────────────────────────────┘
//                              │  (new sum when in_valid && in_ready)
//                              │
//                              ▼
//                      ┌────────────────┐          Handshake to consumer
//                      │  OUT BUF       │───────── out_sum
//                      │  (1 entry)     │───────── out_valid
//   refill prefer      │  out_buf_data  │
//   SPILL → OUT first  │  out_buf_valid │◀──────── out_ready
//                      └───────┬────────┘
//                              │ if out_buf_ready (= !out_buf_valid || out_ready)
//                              │    take from SPILL, else take fresh input
//                              │
//                              ▼
//                      ┌────────────────┐
//                      │  SPILL BUF     │   (2nd slot, used only when OUT is busy)
//                      │  spill_data    │
//                      │  spill_valid   │
//                      └────────────────┘
//
// Notes:
// * Capacity = 2 entries: OUT buffer (consumer-facing) + SPILL buffer (one extra).
// * Backpressure: in_ready = !spill_valid  (no out_ready→in_ready combo path).
// * Refill priority: drain SPILL to OUT first, else accept new input into OUT.
// * If OUT cannot accept and SPILL is free, accept new input into SPILL.
// * When consumer completes a transfer (out_valid && out_ready), OUT may be refilled
//   in the same cycle from SPILL or directly from input.
//==============================================================================
module adder_rv_simple #(
  parameter int W = 32
)(
  input  logic         clk,
  input  logic         rst_n,

  // Producer → adder
  input  logic         in_valid,
  output logic         in_ready,
  input  logic [W-1:0] in_a,
  input  logic [W-1:0] in_b,

  // Adder → consumer
  output logic         out_valid,
  input  logic         out_ready,
  output logic [W-1:0] out_sum
);

  // Main output buffer (feeds the consumer directly)
  // Main output buffer (out_buf_*) is the stage that drives the consumer.
  logic         out_buf_valid;
  logic [W-1:0] out_buf_data;

  // Spill buffer (second slot, holds data if out_buf is occupied)
  logic         spill_buf_valid;
  logic [W-1:0] spill_buf_data;

  // Outputs always come from the main output buffer
  assign out_valid = out_buf_valid;
  assign out_sum   = out_buf_data;

  // out_buf can accept new data if it’s empty OR the consumer is taking it
  wire out_buf_ready = !out_buf_valid || out_ready;

  // We can accept a new input if the spill buffer is free
  assign in_ready = !spill_buf_valid;

  // Buffering logic
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // On reset: clear all valid flags and data
      out_buf_valid   <= 1'b0; // output buffer starts empty
      spill_buf_valid <= 1'b0; // spill buffer starts empty
      out_buf_data    <= '0;
      spill_buf_data  <= '0;
    end else begin
      // -----------------------------------------------------------------
      // Case 1: If the output buffer (out_buf) can accept new data
      //         (either empty, or consumer is taking it right now)
      // -----------------------------------------------------------------
      if (out_buf_ready) begin
        if (spill_buf_valid) begin
          // Prefer to drain from the spill buffer first:
          // move spill data into the output buffer.
          out_buf_data    <= spill_buf_data;
          out_buf_valid   <= 1'b1;
          spill_buf_valid <= 1'b0; // spill slot is now free
        end else if (in_valid) begin
          // If no spill data, but new input arrives:
          // compute sum immediately into the output buffer.
          out_buf_data  <= in_a + in_b;
          out_buf_valid <= 1'b1;
        end else begin
          // No refill available. If consumer is pulling data,
          // mark the output buffer as empty.
          if (out_ready)
            out_buf_valid <= 1'b0;
        end
      end

      // -----------------------------------------------------------------
      // Case 2: If input arrives but output buffer is *not* ready
      //         (full and consumer not pulling), we try to use spill_buf.
      // -----------------------------------------------------------------
      if (in_valid && !out_buf_ready && !spill_buf_valid) begin
        // Stash this new input into the spill buffer.
        // This acts like a "second slot" behind out_buf.
        spill_buf_data  <= in_a + in_b;
        spill_buf_valid <= 1'b1;
      end
    end
  end

endmodule
