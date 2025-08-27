# Simple Verilator CoSim — `adder_rv_simple`

A tiny, portable Verilator‑based co‑simulation demo that verifies a ready/valid **pipelined adder** (`adder_rv_simple`) using a C++ testbench.  
The test drives both **directed vectors** and **random streaming with backpressure**, checks results with a scoreboard, dumps an **FST waveform**, and writes **coverage**—all via simple `make` targets.

---

## What’s inside

```
.
├─ rtl/
│  └─ adder_rv_simple.sv        # 2‑entry elastic adder with ready/valid handshake
├─ sim/
│  └─ tb_main.cpp               # C++ testbench: reset, directed, randomized, scoreboard
├─ logs/                        # Created at runtime: wave.fst, coverage.dat, cov_annotate/
└─ Makefile                     # One‑command build/run/wave/coverage/clean
```

**Key behaviors**
- **Clock & reset** sequencing, then **directed smoke tests**, then **randomized** traffic with ready/valid handshakes and a queue‑based scoreboard.
- **Waveform trace** to `logs/wave.fst` (FST) and **coverage dump** to `logs/coverage.dat`.
- Clean, reproducible run via `make run` (and `make wave`, `make coverage`).

> Tip: The testbench enables FST tracing and writes coverage with Verilator’s APIs, so you don’t need environment magic—just run `make`.


## Requirements

- **Verilator** ≥ 5.031 (tested)
- **C++ compiler** (g++/clang++) and **make**
- **GTKWave** (optional, for viewing waveforms)

Check your Verilator version:
```bash
make version
```

## Quick start

Build + run everything (compiles the model and executes the C++ testbench):
```bash
make run
```
Expected outputs:
- Waveform: `logs/wave.fst`
- Coverage: `logs/coverage.dat`

Open the waveform in GTKWave:
```bash
make wave
```

Generate a coverage report (and source annotation in `logs/cov_annotate/`):
```bash
make coverage
```

Housekeeping:
```bash
make clean      # remove build artifacts (obj_dir/)
make distclean  # also remove logs/
```

### Optional: pin threads with NUMA (Linux)
If you want to pin the simulation and memory to a NUMA node during run:
```bash
make run-numa
```

## End-to-End CoSim Flow

```text
+------------------+       +--------------------+       +-----------------------+
| make run         |  ---> | Verilator C++ build|  ---> | ./obj_dir/sim_<TOP>   |
|                  |       | (obj_dir, O3,      |       | (executable)          |
|                  |       |  threads/tracing)  |       +-----------+-----------+
+------------------+       +--------------------+           | executes
                                                            v
                                            +---------------+-------------------+
                                            |  Testbench main (tb_main.cpp)     |
                                            |  -------------------------------- |
                                            |  1) Apply reset                   |
                                            |  2) Directed vectors (sanity)     |
                                            |  3) Random stream & backpressure  |
                                            |  4) Scoreboard compare            |
                                            |  5) End-of-test summary           |
                                            +---------------+-------------------+
                                                            |
                            +-------------------------------+-------------------------------------+
                            |                                                                     |
                            v                                                                     v
                 +----------------------+                                             +-----------------------+
                 | Wave dump (FST)      |                                             | Coverage dump (DAT)   |
                 | -> logs/wave.fst     |                                             | -> logs/coverage.dat  |
                 +----------+----------+                                              +-----------+-----------+
                            |                                                                     |
                            +-------------------------------------+-------------------------------+
                                                                  |
                                                                  v
                                                        +---------------------+
                                                        | make coverage       |
                                                        +----------+----------+
                                                                   |
                                                                   v
                                                     +-------------------------------+
                                                     | Coverage summary +            |
                                                     | annotated sources:            |
                                                     | logs/cov_annotate/*           |
                                                     +-------------------------------+
```

> Tip: `TOP=adder_rv_simple` is set in the Makefile; artifacts live in `logs/`.

## Design & testbench at a glance

### Handshake summary (DUT)
- **Inputs:** `clk`, `rst_n`, `in_valid`, `in_a[W-1:0]`, `in_b[W-1:0]`, `out_ready`
- **Outputs:** `in_ready`, `out_valid`, `out_sum[W-1:0]`
- Transfer occurs when `in_valid && in_ready` (accept) and when `out_valid && out_ready` (send).

### What the testbench does
- Drives reset for a few cycles, then runs **directed** vectors (e.g., `0+0`, `1+1`, `max+1`, etc.).
- Switches to **random** traffic: ~70% chance to assert `in_valid`, ~60% chance consumer is ready each cycle.
- Keeps a **queue of expected sums**; compares on each actual transfer.
- Dumps **FST** waveforms and writes **coverage** automatically at the end.

## Tuning & notes

- The top module is set to `adder_rv_simple` in the `Makefile` (variable `TOP`).
- Parallelism knobs (also in `Makefile`):
  - `J` = compile parallelism for the C++ build (`-j`)
  - `THREADS` = Verilator worker threads for the model
  - `TRACE_THREADS` = helper threads for FST writer
- Outputs always go into `./logs/` for easy cleanup and inspection.


## Simulation Output (screenshot)

Below is a real console capture showing a full run with **`make run`** followed by **`make coverage`**:

![Simulation run and coverage output](./cosim_output.png)

> The screenshot confirms the expected artifacts and shows the coverage summary (e.g., `Total coverage ... 82.00%`) and the annotation directory hint.

## Troubleshooting

- **No `logs/wave.fst`**  
  Ensure you ran `make run` successfully. The testbench opens `logs/wave.fst` before the test loop.
- **Empty/old coverage**  
  Run `make run` first; `make coverage` consumes the `logs/coverage.dat` file to generate annotated sources.
- **GTKWave not found**  
  Install `gtkwave`, or open the FST file manually in your preferred viewer.



