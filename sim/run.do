# sim/run.do
vlib work
# Compile dut.v and tb_top.v as SystemVerilog with signal-access enabled for debugging,
# so one can inspect and read nets and registers during simulation.
vlog -sv +acc=rn rtl/dut.v rtl/tb_top.v

# Run simulation in command-line mode (-c), suppress GUI, print messages to stdout
vsim -c -quiet tb_top -do {
    run -all;
    quit -f;
}
