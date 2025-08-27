# sim/Makefile — cocotb + Verilator (FST waves, optional threads)

SIM                  ?= verilator
VERILATOR_COVERAGE   ?= verilator_coverage
TOPLEVEL_LANG        ?= verilog

TOPLEVEL       		?= adder_rv_simple
VERILOG_SOURCES 	= ../rtl/adder_rv_simple.sv

MODULE         		?= test_adder_rv_simple

WAVES                 ?= 1
VERILATOR_TRACE        ?= 1
VERILATOR_TRACE_FORMAT ?= fst
VERILATOR_TRACE_DUMPER ?= fst

THREADS       ?= 4
TRACE_THREADS ?= 2

# --- Verilator build/run args
EXTRA_ARGS   += --trace-fst --trace-threads $(TRACE_THREADS)
EXTRA_ARGS   += --threads $(THREADS)

# Compile-time instrumentation (OK)
EXTRA_ARGS += --coverage

# --- Coverage paths
LOGDIR       := logs
COVERAGE_DAT := $(LOGDIR)/coverage.dat
PLUSARGS    += +verilator+coverage+file+$(COVERAGE_DAT)

# Optional: set where cocotb will write the FST name inside sim_build/
export TRACE_FILE ?= wave.fst

# Use cocotb's generic rules
include $(shell cocotb-config --makefiles)/Makefile.sim

.PHONY: run waves coverage

# Ensure logs/ exists before building/running
run: sim

$(LOGDIR):
	@mkdir -p $(LOGDIR)

sim: | $(LOGDIR)

waves:
	@echo "Open waves with: gtkwave sim_build/$(TRACE_FILE) &"
	gtkwave sim_build/$(TRACE_FILE) &

coverage: sim
	@mkdir -p $(LOGDIR) $(LOGDIR)/cov_annotate
	@test -s $(COVERAGE_DAT) || { echo "No $(COVERAGE_DAT) (did the sim finish?)"; exit 1; }
	$(VERILATOR_COVERAGE) --write-info max \
	  --annotate $(LOGDIR)/cov_annotate $(COVERAGE_DAT)
	@echo "Annotated sources in: $(LOGDIR)/cov_annotate/"

