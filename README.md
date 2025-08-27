
# cosim_demo — Small, Practical Co‑Simulation Recipes (File‑based, Verilator C++, and Cocotb+Verilator)

This repo collects **minimal but production‑ready** patterns for HW/SW co‑simulation around a simple ready/valid **adder** DUT. It showcases three complementary approaches:

- **File‑based CoSim** — tool‑agnostic, vendor‑sim compatible (e.g., Questa/ModelSim), great for golden‑vector tests.
- **Verilator‑based CoSim (C++)** — native C++ harness driving a Verilated model every cycle; fast, programmable, with FST wave + coverage.
- **Cocotb + Verilator (Python)** — Python testbench (coroutines) running on the Verilator backend; expressive, and quick to write.

All examples are intentionally tiny so readers can lift the pattern into their own designs with minimal edits.

> **TOPS:** Across examples the adder module is a ready/valid variant (e.g., `adder_rv_simple`).


---

## Repository layout (examples)

```
cosim_demo/
├─ file-based/
│  └─ simple_adder_rv/           # File/CSV‑driven vectors ↔ Verilog TB ↔ DUT
│
└─ verilator-based/
   └─ rv_adder_example/          # C++ testbench ↔ Verilated DUT (FST + coverage)
   │
   └─ cocotb_rv_adder/
      ├─ rtl/                    # DUT (e.g., simple_adder_rv.sv)
      ├─ sim/                    # cocotb Python tests (coroutines, scoreboard)
      └─ Makefile                # runner glue (SIM=verilator, etc.)
```

Each example folder contains a `Makefile` (or — config) with one‑command flows. This **top‑level README** summarizes intent and contrasts the flows.

---

## Why three approaches?

| Aspect                           | File‑based CoSim                                              | Verilator‑based CoSim (C++)                              | Cocotb + Verilator (Python)                                  |
|----------------------------------|---------------------------------------------------------------|----------------------------------------------------------|--------------------------------------------------------------|
| **Integration boundary**         | **Files** between C and Verilog TB                           | **In‑process C++ API** ⇄ Verilated model                 | **Python coroutines** ⇄ Verilator via cocotb FFI             |
| **Authoring language**           | C/C++ for vector gen/check; Verilog TB for I/O                    | C++ testbench + Verilog DUT                              | **Python** testbench + Verilog DUT                           |
| **Timing fidelity**              | Transaction/time‑stepped; cycle sync is manual                | **Cycle‑accurate** (`eval()` per cycle)                  | **Cycle‑accurate** (per‑cycle drives/awaits)                 |
| **Backpressure / handshakes**    | Awkward (pre‑scripted)                                        | Natural (toggle `ready/valid` each cycle)                | Natural (await on `ready`, randomize `valid`)                |
| **Performance**                  | Slower (disk I/O, process barriers)                           | **Fast** (no file I/O; multithreaded Verilator)          | Fast; Python overhead acceptable for unit/regression scope   |
| **Tooling**                      | Any vendor sim (Questa/ModelSim, etc.)                        | Verilator (OSS), C++17 toolchain                         | Verilator (OSS), Python 3.8+, `cocotb` |
| **Waveforms**                    | Vendor formats (WLF/VCD), viewer varies                       | **FST** with GTKWave                                     | VCD/FST via Verilator tracing; GTKWave                       |
| **Coverage**                     | Vendor coverage tools                                         | Verilator `--coverage` + `verilator_coverage`            | Verilator coverage (enable via extra args) + same tooling    |
| **Best for**                     | Golden vectors, portability to many simulators                | High‑iteration fuzzing, CI perf runs, native C/C++       | High‑level tests, quick protos, Python ecosystems            |

Keep **file‑based** for tool‑agnostic golden checks, **Verilator C++** for maximum speed/control, and **cocotb** for Python productivity (with Verilator’s speed).

---

## Example A — File‑based CoSim (`file-based/simple_adder_rv`)

### What it demonstrates
- A **tiny C++ test** emits input vectors (e.g., `a,b`) to a `.dat/.csv` file and expects outputs (`sum`).
- A **Verilog testbench** reads vectors (e.g., `$fscanf`), drives the DUT, captures DUT results to an output file, and ends with a PASS/FAIL banner.
- The C++ side **compares** actual vs. golden and returns the exit status.

### CoSim Flow Diagram
```text
+-----------------+       write        +--------------------+     drive      +------------------+
| C/CPP generator |  --------------->  | Verilog testbench  |  ----------->  |   DUT (adder)    |
|  + checker      |   inputs.dat       |  (file I/O)        |   signals      |  ready/valid     |
+--------+--------+                    +----------+---------+                +--------+---------+
         |  read & compare                         | write results                    |
         +-----------------------------------------+----------------------------------+
                                              outputs.dat
```

### Typical workflow
```bash
# Build + run (uses your installed vendor simulator, e.g., Questa/ModelSim)
make run

# View waves (format depends on your simulator setup, e.g., WLF/VCD)
make wave

# Cleanup
make clean
```

**Notes**
- This pattern is **portable** across simulators because the contract is “just files.”
- Great for **golden‑vector** testing in CI where you want stable, deterministic checks.
- For handshakes/backpressure, vectors are pre‑scripted (less dynamic than cycle‑exact co‑driving).


---

## Example B — Verilator‑based CoSim (C++) (`verilator-based/rv_adder_example`)

### What it demonstrates
- A **C++ testbench** clocks the model, drives `in_valid/ready`, randomizes `out_ready`, and pushes expected sums into a **scoreboard**.
- **Waveform (FST)** is dumped to `logs/wave.fst`; **coverage** is written to `logs/coverage.dat`.
- The provided `Makefile` enables **threads**, **FST writer threads**, and **coverage** out of the box.

### Quick start
```bash
# Build & run (compiles the model, then executes C++ test)
make run

# Open the waveform (GTKWave)
make wave

# Summarize/annotate coverage
make coverage

# Optional: pin to a NUMA node for repeatability (Linux)
make run-numa

# Cleanup
make clean        # remove obj_dir
make distclean    # remove obj_dir and logs/
```

### CoSim Flow Diagram
```text
+------------------+       +--------------------+       +-----------------------+
| make run         |  ---> | Verilator C++ build|  ---> | ./obj_dir/sim_<TOP>   |
|                  |       | (O3, threads, FST) |       | (executable) executes |
+------------------+       +-------------------+        +------+----------------+ 
                                                               v
                                            +------------------+---------------+
                                            |  tb_main.cpp (testbench)         |
                                            |  1) Reset                        |
                                            |  2) Directed smoke tests         |
                                            |  3) Random backpressure          |
                                            |  4) Scoreboard compare           |
                                            |  5) PASS/FAIL & finalize         |
                                            +---------------+------------------+
                                                            |
                           +--------------------------------+--------------------------------------+
                           v                                                                       v
                 +----------------------+                                             +-----------------------+
                 | Wave (FST)           |                                             | Coverage (DAT)        |
                 | logs/wave.fst        |                                             | logs/coverage.dat     |
                 +-----------+----------+                                             +-----------+-----------+
                             |                                                                    |
                             +------------------------------------+-------------------------------+
                                                                  |
                                                                  v
                                                        +---------------------+
                                                        | make coverage       |
                                                        +----------+----------+
                                                                   |
                                                                   v
                                                     +-------------------------------+
                                                     | Summary + annotated sources   |
                                                     | logs/cov_annotate/*           |
                                                     +-------------------------------+
```

### Simulation Output (screenshot)
A real console capture of `make run` followed by `make coverage`:

![Verilator run & coverage](./verilator-based/rv_adder_example/cosim_output.png)

> You should see `logs/wave.fst` and `logs/coverage.dat` produced, then a coverage summary with annotated sources in `logs/cov_annotate/`.

---

## Example C — Cocotb + Verilator (Python) (`cocotb_rv_adder`)

### What it demonstrates
- A **Python cocotb** testbench drives/awaits the ready/valid handshake as coroutines, with a **Python scoreboard** for expected sums.
- Runs on the **Verilator** backend (fast) while letting you author tests in Python (concise randomized traffic, property checks, fixtures).
- Can emit **VCD/FST waves** and (if compiled with `--coverage`) produce coverage files consumable by `verilator_coverage`.

### Quick start
Use the classic **Make-based** cocotb runner:
```bash
make run
```

Or manually pass arguments:
```bash
cd cocotb_rv_adder
make SIM=verilator TOPLEVEL=adder_rv_simple MODULE=test_adder_rv_simple
# Optional: enable waves and threads
make SIM=verilator TOPLEVEL=adder_rv_simple MODULE=test_adder_rv_simple \
     VERILATOR_TRACE=1 VERILATOR_TRACE_FORMAT=fst \
     EXTRA_ARGS="--trace-fst --threads 4 --trace-threads 2"
```


### Simulation Flow

```text
+---------------------------+
| make SIM=verilator ...    |
+-------------+-------------+
              |
              v
   +----------+-----------+
   | cocotb Makefiles     |
   | (set env, generate   |
   | Verilator build cmds)|
   +----------+-----------+
              |
              v
   +----------+-----------+
   | Verilator compile    |
   |  --cc DUT + VPI shim |
   |  --exe cocotb glue   |
   +----------+-----------+
              |
              v
   +----------+-----------+
   | Verilated executable |
   | (cocotb VPI linked)  |
   +----------+-----------+
              |
              v
   +----------+-----------+
   | cocotb scheduler     |
   |  - start_clock()     |
   |  - reset()           |
   |  - drive/await ready |
   |  - scoreboard check  |
   +----------+-----------+
              |
       +------+--------------------------+
       |                                 |
       v                                 v
+-------------+                +---------------------+
| Wave (VCD/  |                | Coverage (optional) |
|  FST)       |                | --coverage -> .dat  |
+------+------+                +---------+-----------+
       |                                 |
       +-------------+-------------------+
                     |
                     v
             PASS/FAIL summary
```

> Enable tracing with `VERILATOR_TRACE=1` (and `VERILATOR_TRACE_FORMAT=fst`), and coverage via `EXTRA_ARGS+="--coverage"`.


**Coverage (optional, if compiled with --coverage):**
```bash
# After a cocotb+verilator run that emitted coverage.dat
verilator_coverage --write-info max --annotate logs/cov_annotate logs/coverage.dat
```

### Notes
- In Make‑based flows, setting `VERILATOR_TRACE=1` enables tracing; `VERILATOR_TRACE_FORMAT=fst` requests FST instead of VCD.
- Python is great for **rapid stimulus**, randomized regressions, and leveraging libraries (numpy/hypothesis, etc.).
- For very long runs, prefer the C++ harness for peak throughput; keep cocotb for expressive unit/regression tests.


---

## Migrating the patterns to your own DUT

- **Swap the DUT**: replace the adder with your RTL and update the test stimulus/golden model.
- **Extend the scoreboard**: encode your functional model in C/C++ or Python; compare on each handshake.
- **Grow the protocol**: expand the ready/valid pair to AXI‑Stream, add sideband signals, or layer transactions.
- **Scale the tests**: leverage the Verilator **C++** flow for fuzz/randomized constraints; keep **file‑based** vectors as golden smoke tests; use **cocotb** for productivity and Python ecosystem leverage.

---

## FAQ / Troubleshooting

**Q: Verilator example runs but there’s no `logs/wave.fst`.**  
A: Ensure the test completed; the harness opens the FST early and closes it on exit. Check write permissions to `logs/`.
For cocotb, set `VERILATOR_TRACE=1` (and optionally `VERILATOR_TRACE_FORMAT=fst`).

**Q: Coverage report is empty.**  
A: Run a simulation compiled with `--coverage` first; `verilator_coverage` then reads `logs/coverage.dat`.
For cocotb, pass `EXTRA_ARGS+="--coverage"`.

**Q: I only have a vendor simulator.**  
A: Use the **file‑based** example; the pattern is simulator‑agnostic.

**Q: How do I add backpressure in file‑based tests?**  
A: Encode “ready” cycles as a column in your vector file and have the Verilog testbench respect them while reading inputs.

---

## License

MIT.

---

## Acknowledgements

Thanks to the open‑source Verilator, cocotb, GTKWave communities. This repo is intentionally small to make the patterns easy to transplant.
