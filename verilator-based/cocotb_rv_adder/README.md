# cocotb + Verilator Ready/Valid Adder Example

This example shows how to verify a **SystemVerilog** ready/valid adder (`adder_rv_simple.sv`) using a **cocotb** Python testbench on a **Verilated C++** simulator. It includes FST waveform dumping and Verilator **coverage** with post-run source annotation.

---

## Project layout

```
.
├── rtl/
│   └── adder_rv_simple.sv
└── sim/
    ├── Makefile
    └── test_adder_rv_simple.py
```

- `rtl/adder_rv_simple.sv` — 2-slot elastic ready/valid adder (OUT + SPILL buffers).
- `sim/Makefile` — drives Verilator build, runs cocotb, enables FST waves & coverage.
- `sim/test_adder_rv_simple.py` — cocotb test with a POP→ENQ scoreboard and phase-clean driving/sampling.

After a run, you will see:
- `sim/sim_build/` — the Verilated C++ build and `Vtop` executable plus `wave.fst`
- `sim/logs/coverage.dat` — raw coverage database
- `sim/logs/cov_annotate/` — annotated sources (created by `make coverage`)

---

## Quick start

```bash
# From the repo root
cd sim

# Build + run the test (cocotb + Verilator)
make

# View waves (requires GTKWave)
make waves

# Generate coverage annotation (requires verilator_coverage)
make coverage
```

**Notes**
- The Makefile compiles with `--trace-fst --threads N --trace-threads M --coverage` and runs with `+verilator+coverage+file+logs/coverage.dat` so coverage is flushed on normal exit.
- Waves go to `sim_build/wave.fst` by default (override via `TRACE_FILE=myrun.fst make`).

---

## Architecture (ASCII diagram)

```
+-----------------------------+
|         Python world        |
|  cocotb tests (@cocotb.test)|
+-------------+---------------+
              |
              |  (triggers, scheduling)
              v
+-------------+---------------+
|        cocotb scheduler     |
+-------------+---------------+
              |
              |  (VPI calls & callbacks)
              v
+-------------+---------------+
|        cocotb VPI shim      |
|     (GPI ↔ VPI adapter)     |
+-------------+---------------+
              |
              v
+--------------------------------------------------------------+
|               Verilated C++ simulator (Vtop)                 |
|  +-------------------+   +-----------------+   +-----------+ |
|  | Verilated DUT     |   | Verilator RT    |   |  VPI impl | |
|  | (adder_rv_simple) |   | (time / tracing)|   | (cb/value)| |
|  +---------+---------+   +-----------------+   +-----------+ |
|            |                                              |  |
|            |  drives/reads via vpi_put/get_value          |  |
|            |                                              |  |
|   [FST wave writer]  ─────────►  sim_build/wave.fst       |  |
|   [Coverage counters] ──(on exit)──► logs/coverage.dat    |  |
+--------------------------------------------------------------+
```

**Flow in words**
1. Verilator translates the RTL to C++ and links an executable `sim_build/Vtop` with VPI enabled.
2. At startup, `Vtop` loads cocotb’s VPI library (startup hook) → Python interpreter starts → the test module is imported.
3. Tests `await` triggers; cocotb registers VPI callbacks (e.g., clock edges, read‑only phase) and resumes coroutines at the right time.
4. Python writes/reads ports via VPI (`vpi_put_value` / `vpi_get_value`). Waves stream to `wave.fst`; coverage dumps to `coverage.dat` on exit.

---

## Per‑cycle handshake & scoreboard (ASCII timing)

```
Time → → →

Cycle k
---------[ FallingEdge ]--------------------------------[ RisingEdge ]--------
TB drive:  in_valid/in_a/in_b/out_ready
           (writes happen here; legal to drive)

                         TB snapshot: ReadOnly()  (pre-edge view of signals)
                         - pre_in_valid, pre_in_ready
                         - pre_out_valid, pre_out_ready
                         - pre_a, pre_b, pre_out_sum

                         Scoreboard (software):
                         - POP first if (pre_out_valid & pre_out_ready)
                         - ENQ second if (pre_in_valid & pre_in_ready)
                           ENQ uses BUS values (pre_a + pre_b)

                                    DUT transfers occur on the edge
                                    (out pop and/or in accept happen here)
-----------------------------------------------------------------------------
Next cycle k+1 repeats: drive on FallingEdge → ReadOnly snapshot → POP→ENQ → RisingEdge
```

**Why this is robust**
- **No writes** in read-only: all DUT assignments are made before the edge (at FallingEdge).
- **Pre-edge truth**: decisions use the values the DUT will actually see at the upcoming posedge.
- **POP → ENQ** handles elastic pipelines that can drain and refill on the **same** edge.

---

## Makefile knobs you can tweak

- `THREADS` / `TRACE_THREADS` — parallelize model evaluation and FST compression.
- `TRACE_FILE` — change wave filename: `TRACE_FILE=myrun.fst make`.
- `EXTRA_ARGS` — add Verilator switches (e.g., `-O3`, `--x-assign fast`).
- `PLUSARGS` — runtime options; e.g., move coverage file: \
  `PLUSARGS='+verilator+coverage+file+/abs/path/coverage.dat' make`.

---

## Troubleshooting

- **coverage.dat missing** → Run `make` first; coverage writes on **clean exit**. Ensure `logs/` exists.
- **“Unknown runtime argument: +verilator+coverage+1”** → Remove it (not used in Verilator 5.x). Keep `--coverage` at compile time; use `+verilator+coverage+file+...` at runtime.
- **“Write during read-only phase”** → Only **write** on a driving phase (e.g., right after `FallingEdge`); after `ReadOnly()` you must only sample.

---

## Commands recap

```bash
cd sim && make          # build & run
make waves              # open sim_build/wave.fst
make coverage           # annotate coverage
```

Happy simulating! 🚀
