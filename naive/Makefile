# Makefile
CC       := gcc
CFLAGS   := -O2 -Wall -Wextra
VSIM_BIN := vsim

all: run

build:
	@mkdir -p build

sw: build
	$(CC) $(CFLAGS) -o build/sw sw/main.c

run: sw
	@rm -f sim/out.dat
	@echo "[Make] Running C testbench → will invoke $(VSIM_BIN)"
	@PATH="$$PATH" build/sw

clean:
	@rm -rf build work transcript *.wlf
	@rm -f sim/out.dat sim/in.dat

.PHONY: all build sw run clean
