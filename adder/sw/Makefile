# Makefile — Build & run the C++ file-based co-sim testbench (Questa/ModelSim)
# Linux-only. Requires g++ (C++17) and Questa/ModelSim (vlib/vlog/vsim) on PATH.
#
# Usage:
#   make            # build cosim_tb
#   make run        # build + run (invokes vlib/vlog/vsim via the host)
#   make clean      # remove build artifacts, work lib, inputs/outputs
#   make help       # print this help
#
# One can override any variable on the make command line, e.g.:
#   make COSIM_N=4096 COSIM_SEED=7
#   make RTL=../rtl/adder_rv_simple.sv TB=../rtl/adder_cosim_tb.sv
#

# -------- Toolchain --------
CXX      ?= g++
CXXFLAGS ?= -O2 -std=gnu++17 -Wall -Wextra -Wpedantic
LDFLAGS  ?=
FS_LIB   ?=            # set to -lstdc++fs if needed with old GCC
LDLIBS   ?= $(FS_LIB)

# -------- Sources / Target --------
TARGET   ?= cosim_tb
SOURCES  ?= cosim_tb.cpp

# -------- Co-sim configuration (compiled into the host via -D macros) --------
COSIM_N       ?= 1024
COSIM_SEED    ?= 1

RTL           ?= ../rtl/adder_rv_simple.sv
TB            ?= ../rtl/adder_cosim_tb.sv

INPUT_FILE    ?= ./inputs.txt
OUTPUT_FILE   ?= ./outputs.txt
WORKDIR       ?= ./.cosim_q

VLOG          ?= vlog
VSIM          ?= vsim

# -------- Preprocessor defines passed to the host build --------
DFLAGS += -DCOSIM_N=$(COSIM_N) -DCOSIM_SEED=$(COSIM_SEED)
DFLAGS += -DCOSIM_RTL_PATH=\"$(RTL)\"
DFLAGS += -DCOSIM_TB_PATH=\"$(TB)\"
DFLAGS += -DCOSIM_INPUT_FILE=\"$(INPUT_FILE)\"
DFLAGS += -DCOSIM_OUTPUT_FILE=\"$(OUTPUT_FILE)\"
DFLAGS += -DCOSIM_WORKDIR=\"$(WORKDIR)\"
DFLAGS += -DCOSIM_VLOG=\"$(VLOG)\"
DFLAGS += -DCOSIM_VSIM=\"$(VSIM)\"

# -------- Default target --------
.PHONY: all
all: $(TARGET)

# -------- Build --------
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(DFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# -------- Run (build + execute the host) --------
.PHONY: run
run: $(TARGET)
	./$(TARGET)

# -------- Utilities --------
.PHONY: clean
clean:
	@echo "Cleaning…"
	@rm -rf $(TARGET) $(WORKDIR) work transcript vsim.wlf \
	        $(INPUT_FILE) $(OUTPUT_FILE)

.PHONY: help
help:
	@grep -E '^\#( |$$)|^[a-zA-Z0-9_-]+:.*$$' Makefile | sed -E 's/^\# ?//'

.PHONY: vars
vars:
	@echo "CXX=$(CXX)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "TARGET=$(TARGET)"
	@echo "SOURCES=$(SOURCES)"
	@echo "COSIM_N=$(COSIM_N)"
	@echo "COSIM_SEED=$(COSIM_SEED)"
	@echo "RTL=$(RTL)"
	@echo "TB=$(TB)"
	@echo "INPUT_FILE=$(INPUT_FILE)"
	@echo "OUTPUT_FILE=$(OUTPUT_FILE)"
	@echo "WORKDIR=$(WORKDIR)"
	@echo "VLOG=$(VLOG)"
	@echo "VSIM=$(VSIM)"
	@echo "DFLAGS=$(DFLAGS)"
