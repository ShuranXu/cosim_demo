################################################################################
# Makefile — Verilator 5.031, threading + FST tracing + coverage + O3
################################################################################

# Tools
VERILATOR           ?= verilator
VERILATOR_COVERAGE  ?= verilator_coverage
GTKWAVE             ?= gtkwave

# Parallelism
J        ?= 8            # compile parallelism (-j) for the C++ build
THREADS  ?= 4            # Verilator worker threads for the DUT: --threads N
TRACE_THREADS ?= 2       # FST writer helper threads (0..2): --trace-threads M
THREADS_DPI  ?= all      # DPI safety for threads: pure|all|none  (usually 'all')

# Project layout
TOP      := adder_rv_simple
RTL_DIR  := rtl
SIM_DIR  := sim
BUILD    := obj_dir
BIN      := sim_$(TOP)

# Sources
RTL_SRCS := $(RTL_DIR)/$(TOP).sv     # change to adder_rv_simple.sv if using that top
TB_SRC   := $(SIM_DIR)/tb_main.cpp

# Verilator & compiler flags
VERI_FLAGS := -Wall --cc --exe --build -j $(J) \
              --threads $(THREADS) --threads-dpi $(THREADS_DPI) \
              --trace-fst --trace-threads $(TRACE_THREADS) \
              --coverage

# (C++ for harness & Verilated model)
CFLAGS := -O3 -DNDEBUG -std=c++17 -I$(shell verilator -getenv VERILATOR_ROOT)/include
LDFLAGS  := -O3

.PHONY: all version build run run-numa wave coverage clean distclean

all: run

version:
	@$(VERILATOR) --version

build: logs
	$(VERILATOR) $(VERI_FLAGS) \
	  -CFLAGS "$(CFLAGS)" -LDFLAGS "$(LDFLAGS)" \
	  -Mdir $(BUILD) -o $(BIN) \
	  -DVL_USER_FINISH   \
	  $(TB_SRC) $(RTL_SRCS)

run: build
	./$(BUILD)/$(BIN)
	@echo "Waveform : logs/wave.fst"
	@echo "Coverage : logs/coverage.dat"

# Optional: pin to a NUMA node / core list for steadier perf numbers
# Edit the CPU list to match your box (e.g., 0-3 for 4 cores).
run-numa: build
	numactl -C 0-$(shell expr $(THREADS) - 1) -m 0 -- ./$(BUILD)/$(BIN)
	@echo "Waveform : logs/wave.fst"
	@echo "Coverage : logs/coverage.dat"

wave:
	$(GTKWAVE) logs/wave.fst &

coverage:
	$(VERILATOR_COVERAGE) --write-info max \
	  --annotate logs/cov_annotate logs/coverage.dat
	@echo "Annotated sources in: logs/cov_annotate/"

logs:
	mkdir -p logs

clean:
	rm -rf $(BUILD)

distclean: clean
	rm -rf logs
