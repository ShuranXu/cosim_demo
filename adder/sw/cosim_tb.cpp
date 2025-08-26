// cosim_tb.cpp
// File-based co-simulation host for Questa/ModelSim.
// Generates inputs, compiles & runs the SV harness, then compares outputs.
//
// Build (defaults):
//   g++ -O2 -std=c++17 -o cosim_tb cosim_tb.cpp
//
// Override examples:
//   g++ -O2 -std=c++17 -o cosim_tb cosim_tb.cpp \
//       -DCOSIM_N=4096 -DCOSIM_SEED=7 \
//       -DCOSIM_RTL_PATH=\"../rtl/adder_rv_simple.sv\" \
//       -DCOSIM_TB_PATH=\"./rv_cosim_tb.sv\" \
//       -DCOSIM_INPUT_FILE=\"./inputs.txt\" \
//       -DCOSIM_OUTPUT_FILE=\"./outputs.txt\" \
//       -DCOSIM_WORKDIR=\"./.cosim_q\" \
//       -DCOSIM_VLOG=\"vlog\" -DCOSIM_VSIM=\"vsim\"
#include <cstdint>
#include <cstdio>
#include <cstdlib>  // std::system
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <sys/wait.h>   // WIFEXITED, WEXITSTATUS

#ifndef COSIM_N
#  define COSIM_N 1024
#endif
#ifndef COSIM_SEED
#  define COSIM_SEED 1
#endif
#ifndef COSIM_RTL_PATH
#  define COSIM_RTL_PATH "../rtl/adder_rv_simple.sv"
#endif
#ifndef COSIM_TB_PATH
#  define COSIM_TB_PATH "./rv_cosim_tb.sv"
#endif
#ifndef COSIM_INPUT_FILE
#  define COSIM_INPUT_FILE "./inputs.txt"
#endif
#ifndef COSIM_OUTPUT_FILE
#  define COSIM_OUTPUT_FILE "./outputs.txt"
#endif
#ifndef COSIM_WORKDIR
#  define COSIM_WORKDIR "./.cosim_q"
#endif
#ifndef COSIM_VLOG
#  define COSIM_VLOG "vlog"
#endif
#ifndef COSIM_VSIM
#  define COSIM_VSIM "vsim"
#endif

static int run_cmd(const std::string& cmd, const std::filesystem::path& cwd) {
    namespace fs = std::filesystem;

    const fs::path prev = fs::current_path();   // remember current dir
    int status = -1;

    try {
        fs::current_path(cwd);                  // cd to target
        status = std::system(cmd.c_str());      // run through /bin/sh -c
        fs::current_path(prev);                 // cd back
    } catch (...) {
        // best effort to restore, then rethrow
        std::error_code ec;
        fs::current_path(prev, ec);
        throw;
    }

    if (status == -1) return -1;                // system() failed
    if (WIFEXITED(status))  return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;                               // fallback
}

static bool write_inputs_txt(const std::filesystem::path& path,
                             const std::vector<std::pair<uint32_t,uint32_t>>& vec) {
    std::ofstream ofs(path);
    if (!ofs) return false;
    // Sets the stream's base to hexadecimal for all subsequent integer writes
    ofs.setf(std::ios::hex, std::ios::basefield);
    for (auto [a,b] : vec) {
        // std::noshowbase → don't print 0x prefix.
        // std::nouppercase → hex letters are lowercase (a..f).
        // std::setw(8) + std::setfill('0') → each number is exactly 8 hex digits,
        // zero-padded on the left.
        ofs << std::noshowbase << std::nouppercase
            << std::setw(8) << std::setfill('0') << a << ' '
            << std::setw(8) << std::setfill('0') << b << '\n';
    }
    return true;
}

static bool read_outputs_txt(const std::filesystem::path& path,
                                std::vector<uint32_t>& out) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    // discard any previous contents
    out.clear();
    std::string tok;
    // Read tokens separated by any whitespace (spaces/newlines/tabs)
    while (ifs >> tok) {
        uint32_t v = 0;
        // parse each token separately
        std::stringstream ss(tok);
        ss.setf(std::ios::hex, std::ios::basefield);
        // interpret token as hexadecimal
        ss >> v;
        // On overflow (value doesn't fit in uint32_t) or malformed input,
        // the stream sets failbit (so ss.fail() becomes true).
        if (!ss.fail()) out.push_back(v);
    }
    return true;
}

int main() {

    using std::filesystem::path;
    const path   RTL    = COSIM_RTL_PATH;
    const path   TB     = COSIM_TB_PATH;
    const path   INP    = COSIM_INPUT_FILE;
    const path   OUT    = COSIM_OUTPUT_FILE;
    const path   WORK   = COSIM_WORKDIR;
    const std::string VLOG = COSIM_VLOG;
    const std::string VSIM = COSIM_VSIM;

    std::error_code ec;
    std::filesystem::create_directories(WORK, ec);

    // Stimulus
    std::vector<std::pair<uint32_t,uint32_t>> in;
    in.reserve(COSIM_N);
    uint32_t s = (uint32_t)COSIM_SEED;
    auto lcg = [&](){ s = s * 1664525u + 1013904223u; return s; };
    for (int i = 0; i < COSIM_N; ++i) {
        uint32_t a = lcg();
        uint32_t b = lcg();
        in.emplace_back(a, b);
    }
    if (!write_inputs_txt(INP, in)) {
        std::cerr << "[C-TB] ERROR: cannot write " << INP << "\n";
        return 2;
    }
    std::filesystem::remove(OUT, ec);

    // Paths for defines (absolute, to avoid cwd confusion)
    path abs_in  = std::filesystem::absolute(INP);
    path abs_out = std::filesystem::absolute(OUT);

    // 1) vlib work
    if (run_cmd("vlib work", WORK) != 0) { std::cerr << "[C-TB] vlib failed\n"; return 3; }

    // 2) vlog: compile RTL + TB, injecting macros for input/output file paths
    std::stringstream vlog_cmd;
    vlog_cmd << VLOG << " -sv "
             << "+define+COSIM_INPUTS="  << '\"' << abs_in.string()  << '\"' << ' '
             << "+define+COSIM_OUTPUTS=" << '\"' << abs_out.string() << '\"' << ' '
             << '\"' << std::filesystem::absolute(RTL).string() << '\"' << ' '
             << '\"' << std::filesystem::absolute(TB).string()  << '\"';
    if (run_cmd(vlog_cmd.str(), WORK) != 0) { std::cerr << "[C-TB] vlog failed\n"; return 3; }

    // 3) vsim: run batch
    std::stringstream vsim_cmd;
    vsim_cmd << VSIM << " -c work.rv_cosim_tb -do " << '\"' << "run -all; quit -f" << '\"';
    if (run_cmd(vsim_cmd.str(), WORK) != 0) { std::cerr << "[C-TB] vsim failed\n"; return 3; }

    // 4) Compare
    std::vector<uint32_t> got;
    if (!read_outputs_txt(OUT, got)) {
        std::cerr << "[C-TB] ERROR: cannot read " << OUT << "\n";
        return 4;
    }
    if ((int)got.size() != COSIM_N) {
        std::cerr << "[C-TB] ERROR: length mismatch outputs=" << got.size()
                  << " inputs=" << COSIM_N << "\n";
        return 4;
    }

    size_t mism = 0;
    for (int i = 0; i < COSIM_N; ++i) {
        uint32_t exp = (uint32_t)((uint64_t)in[i].first + (uint64_t)in[i].second);
        if (got[i] != exp) {
            if (mism < 10) {
                std::cerr << "MISMATCH @" << i
                          << " a=" << std::hex << std::uppercase << in[i].first
                          << " b=" << in[i].second
                          << " out=" << got[i]
                          << " exp=" << exp << std::dec << "\n";
            }
            ++mism;
        }
    }

    if (mism == 0) {
        std::cout << "[C-TB] PASS: all " << COSIM_N << " matched.\n";
        return 0;
    } else {
        std::cout << "[C-TB] FAIL: " << mism << " mismatches of " << COSIM_N << ".\n";
        return 5;
    }
}
