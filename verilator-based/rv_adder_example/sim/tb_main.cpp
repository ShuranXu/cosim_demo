// sim/tb_main.cpp
// Verilator 5.031: FST tracing + coverage + O3 (set via Makefile)

#include "verilated.h"
#include "verilated_fst_c.h"
#include "verilated_cov.h"
#include "Vadder_rv_simple.h"   // top module name matches rtl/adder_rv_simple.sv

#include <cstdint>
#include <cstdio>
#include <queue>
#include <random>

static vluint64_t main_time = 0;

static inline void dump_step(VerilatedFstC* tfp) {
    tfp->dump(++main_time);   // dump each half-cycle
}

int main(int argc, char** argv) {

    // captures the argc/argv from main() and stores
    // them inside Verilator’s global runtime context
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    // DUT + tracer
    auto* top = new Vadder_rv_simple;
    auto* tfp = new VerilatedFstC;
    top->trace(tfp, /*depth*/ 5);
    tfp->open("logs/wave.fst");

    // Clock/reset
    top->clk   = 0;
    top->rst_n = 0;

    // Handshake I/O init
    top->in_valid  = 0;
    top->in_a      = 0;
    top->in_b      = 0;
    top->out_ready = 0;

    // Scoreboard of expected sums (pushed on accept, popped on send)
    std::queue<uint64_t> expq;

    constexpr unsigned W = 32;
    const uint64_t mask = (W == 64) ? ~0ull : ((1ull << W) - 1);

    // PRNGs (deterministic)
    std::mt19937_64 rng(1);
    auto rand_bit = [&](int prob_percent) -> bool {
        return (int)(rng() % 100) < prob_percent;
    };

    // ---- Reset for a few cycles
    for (int i = 0; i < 4; ++i) {
        top->clk = 0; top->eval(); dump_step(tfp);
        top->clk = 1; top->eval(); dump_step(tfp);
    }
    top->rst_n = 1;

    int errors = 0;

    // ---- Directed smoke: always-accept (no backpressure)
    struct Vec { uint64_t a, b; };
    const Vec dirv[] = {
        {0,0}, {1,0}, {0,1}, {1,1}, {mask,1}, {mask,mask}
    };

    for (auto v : dirv) {
        // Drive inputs on "negedge" phase
        top->clk = 0;
        top->in_valid  = 1;
        top->in_a      = v.a & mask;
        top->in_b      = v.b & mask;
        top->out_ready = 1;                 // consumer always ready here
        top->eval(); dump_step(tfp);

        // If DUT accepted, remember expected value
        if (top->in_valid && top->in_ready) {
            expq.push((v.a + v.b) & mask);
        }

        // "posedge": flops update, output may fire
        top->clk = 1;
        top->eval(); dump_step(tfp);

        // If output fired, compare and pop
        if (top->out_valid && top->out_ready) {
            if (expq.empty()) {
                std::fprintf(stderr, "[DIR] Unexpected output (empty expq)\n");
                ++errors;
            } else {
                uint64_t exp = expq.front(); expq.pop();
                uint64_t got = (uint64_t)top->out_sum;
                if (got != exp) {
                    std::fprintf(stderr, "[DIR] a=%llu b=%llu got=%llu exp=%llu\n",
                        (unsigned long long)v.a, (unsigned long long)v.b,
                        (unsigned long long)got, (unsigned long long)exp);
                    ++errors;
                }
            }
        }
    }

    // Drain any remaining directed outputs (few cycles)
    // recall that the adder is buffered (it has storage), stopping stimulus
    // doesn’t mean the DUT’s output is immediately empty. During the directed tests
    // we may have pushed more items than we popped. So setting and
    // clocking a few cycles lets the DUT emit everything it already accepted.
    for (int i = 0; i < 64 && (!expq.empty() || top->out_valid); ++i) {
        // negedge phase
        top->clk = 0;
        top->in_valid = 0;
        top->out_ready = 1;
        top->eval();
        dump_step(tfp);

        // posedge phase
        top->clk = 1;
        top->eval();
        dump_step(tfp);

        if (top->out_valid && top->out_ready) {
            if (expq.empty()) { std::fprintf(stderr, "[DIR] drain: empty expq\n"); ++errors; }
            else {
                uint64_t exp = expq.front(); expq.pop();
                uint64_t got = (uint64_t)top->out_sum;
                if (got != exp) {
                    std::fprintf(stderr, "[DIR drain] got=%llu exp=%llu\n",
                        (unsigned long long)got, (unsigned long long)exp);
                    ++errors;
                }
            }
        }
    }

    // ---- Randomized streaming with backpressure
    const int cycles = 2000;
    for (int t = 0; t < cycles; ++t) {
        // Decide next inputs and consumer readiness
        bool present = rand_bit(70);   // ~70% chance to assert in_valid
        bool ready   = rand_bit(60);   // ~60% chance consumer ready

        uint64_t a = rng() & mask;
        uint64_t b = rng() & mask;

        // ----- Low phase: drive inputs / readiness for the upcoming edge
        top->clk       = 0;
        top->in_valid  = present;
        top->in_a      = a;
        top->in_b      = b;
        top->out_ready = ready;

        top->eval(); dump_step(tfp);

        // Snapshot *pre-edge* outputs; these decide the pop at this edge
        const bool     pre_valid = top->out_valid;
        const bool     pre_ready = top->out_ready;   // what the consumer will present at the edge
        const uint64_t pre_sum   = (uint64_t)top->out_sum;

        // If the DUT will accept this input at the edge, enqueue expectation
        if (top->in_valid && top->in_ready) {
            expq.push((a + b) & mask);
        }

        // ----- Rising edge: registers update (pop/push happen here)
        top->clk = 1;
        top->eval(); dump_step(tfp);

        // Use the *pre-edge* snapshot to decide/verify the pop
        // A transfer happens on the rising edge when out_valid && out_ready as
        // they were just before that edge.
        // So the value to compare is the pre-edge out_sum, not the one we read
        // after eval() on the posedge (which may already be the next word).
        if (pre_valid && pre_ready) {
            if (expq.empty()) {
                std::fprintf(stderr, "[RND %d] Unexpected output (empty expq)\n", t);
                ++errors;
            } else {
                const uint64_t exp = expq.front(); expq.pop();
                const uint64_t got = pre_sum;  // value that was actually transferred
                if (got != exp) {
                    std::fprintf(stderr, "[RND %d] got=%llu exp=%llu\n",
                                t, (unsigned long long)got, (unsigned long long)exp);
                    ++errors;
                }
            }
        }

        if (VL_UNLIKELY(Verilated::gotFinish())) break;
    }


    // Final drain (keep source idle, let sink pull)
    for (int i = 0; i < 64 && (!expq.empty() || top->out_valid); ++i) {
        top->clk = 0; top->in_valid = 0; top->out_ready = 1; top->eval(); dump_step(tfp);
        top->clk = 1; top->eval(); dump_step(tfp);
        if (top->out_valid && top->out_ready) {
            if (expq.empty()) { std::fprintf(stderr, "[DRN] empty expq\n"); ++errors; }
            else {
                uint64_t exp = expq.front(); expq.pop();
                uint64_t got = (uint64_t)top->out_sum;
                if (got != exp) {
                    std::fprintf(stderr, "[DRN] got=%llu exp=%llu\n",
                        (unsigned long long)got, (unsigned long long)exp);
                    ++errors;
                }
            }
        }
    }

    // Close tracing and write coverage
    tfp->close();
    VerilatedCov::write("logs/coverage.dat");

    delete tfp;
    delete top;

    if (errors) {
        std::fprintf(stderr, "TEST FAIL: %d mismatches\n", errors);
        return 1;
    }
    std::printf("TEST PASS\n");
    return 0;
}
