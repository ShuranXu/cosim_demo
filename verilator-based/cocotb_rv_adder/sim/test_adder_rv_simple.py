# sim/test_adder_rv_simple.py
import random
from collections import deque

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, FallingEdge, ReadOnly

W = 32
MASK = (1 << W) - 1 if W < 64 else (1 << 64) - 1

def rand_bit(rng: random.Random, pct: int) -> bool:
    return rng.randrange(100) < pct

async def reset_dut(dut, cycles=4):
    """Active-low, synchronous reset with full-cycle hold and phase-clean writes."""
    # Safe defaults
    dut.in_valid.value  = 0
    dut.in_a.value      = 0
    dut.in_b.value      = 0
    dut.out_ready.value = 0

    # Assert on a driving phase
    await FallingEdge(dut.clk)
    dut.rst_n.value = 0

    # Hold reset for 'cycles' full clock periods
    for _ in range(cycles):
        await RisingEdge(dut.clk)
        await FallingEdge(dut.clk)

    # Deassert on a driving phase; give one posedge to settle
    dut.rst_n.value = 1
    await RisingEdge(dut.clk)

async def one_cycle(dut, expq: deque, pre_label: str = "", mask: int = MASK):
    """One scoreboard step (no DUT writes here):
       - SNAPSHOT pre-edge (ReadOnly)
       - POP → ENQ using pre-edge handshakes
       - ADVANCE (RisingEdge)
       Returns: (in_accepted: bool, errors_added: int)
    """
    errors_added = 0

    # Pre-edge snapshot (what transfers at the upcoming rising edge)
    # Driver already set inputs on a FallingEdge
    await ReadOnly()
    # Decide what will transfer on the upcoming edge
    pre_in_valid  = int(dut.in_valid.value)
    pre_in_ready  = int(dut.in_ready.value)
    pre_out_valid = int(dut.out_valid.value)
    pre_out_ready = int(dut.out_ready.value)
    pre_out_sum   = int(dut.out_sum.value) & mask
    pre_a         = int(dut.in_a.value) & mask
    pre_b         = int(dut.in_b.value) & mask

    # POP first: output transfer corresponds to the oldest expected item
    if pre_out_valid and pre_out_ready:
        if not expq:
            dut._log.error("[%s] Unexpected output (empty expq)", pre_label)
            errors_added += 1
        else:
            exp = expq.popleft()
            got = pre_out_sum
            if got != exp:
                dut._log.error("[%s] got=%d exp=%d", pre_label, got, exp)
                errors_added += 1

    # ENQ second: input accepted on this edge → enqueue from BUS values
    in_accepted = bool(pre_in_valid and pre_in_ready)
    if in_accepted:
        expq.append((pre_a + pre_b) & mask)

    # Advance DUT to perform the transfers we just accounted for
    await RisingEdge(dut.clk)
    return in_accepted, errors_added

@cocotb.test()
async def test_adder_rv_simple(dut):
    """Ready/valid test with pre-edge POP→ENQ."""
    cocotb.start_soon(Clock(dut.clk, 10, units="ns").start())

    rng = random.Random(1)
    await reset_dut(dut, cycles=4)

    # Optional: prime one edge with sink not-ready to avoid a first-cycle POP
    await FallingEdge(dut.clk)
    dut.out_ready.value = 0
    _, _ = await one_cycle(dut, deque(), pre_label="PRIME")  # temp deque; no effect

    expq = deque()
    errors = 0

    # -------------------------
    # Phase 1: Directed smoke
    # -------------------------
    directed = [
        (0, 0),
        (1, 0),
        (0, 1),
        (1, 1),
        (MASK, 1),
        (MASK, MASK),
    ]

    for (a, b) in directed:
        accepted = False
        while not accepted:
            # DRIVE (FallingEdge): present this vector every cycle until accepted
            await FallingEdge(dut.clk)
            dut.out_ready.value = 1
            dut.in_valid.value  = 1
            dut.in_a.value      = a & MASK
            dut.in_b.value      = b & MASK

            # OBSERVE + SCOREBOARD + ADVANCE
            accepted, e = await one_cycle(dut, expq, pre_label="DIR")
            errors += e

        # Cleanly deassert valid on a FallingEdge
        await FallingEdge(dut.clk)
        dut.in_valid.value = 0
        # Let outputs move a bit
        _, e = await one_cycle(dut, expq, pre_label="DIR post")
        errors += e

    # Drain a few edges
    for _ in range(8):
        await FallingEdge(dut.clk)
        dut.in_valid.value  = 0
        dut.out_ready.value = 1
        _, e = await one_cycle(dut, expq, pre_label="DIR drain")
        errors += e

    # -------------------------
    # Phase 2: Randomized flow
    # -------------------------
    for t in range(2000):
        # DRIVE (FallingEdge)
        await FallingEdge(dut.clk)

        dut.out_ready.value = 1 if rand_bit(rng, 60) else 0

        if rand_bit(rng, 70):
            dut.in_valid.value = 1
            # rng.getrandbits(W) produces a non-negative integer with exactly up to W random bits.
            dut.in_a.value = rng.getrandbits(W) & MASK
            dut.in_b.value = rng.getrandbits(W) & MASK
        else:
            dut.in_valid.value = 0

        # OBSERVE + SCOREBOARD + ADVANCE
        _, e = await one_cycle(dut, expq, pre_label=f"RND {t}")
        errors += e

    # Final drain: stop producing, keep sink ready
    for _ in range(256):
        await FallingEdge(dut.clk)
        dut.in_valid.value  = 0
        dut.out_ready.value = 1
        _, e = await one_cycle(dut, expq, pre_label="FINAL")
        errors += e
        if not expq and int(dut.out_valid.value) == 0:
            break

    if expq:
        dut._log.error("Items left in scoreboard after drain: %d", len(expq))
        errors += 1

    assert errors == 0, f"Test FAILED with {errors} mismatches"
