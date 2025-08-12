#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utils.h"


/**
 * @brief Entry point for the C testbench program.
 *
 * This function sets up a test scenario for validating a DUT (Design Under Test)
 * by generating deterministic input data, invoking a simulator, and comparing
 * the DUT outputs against a golden reference.
 *
 * Workflow:
 *  1. Allocate and initialize input (`in`), golden reference (`gld`), and output (`out`) buffers.
 *  2. Generate deterministic test inputs, including negative values.
 *  3. Write the input data to an external file (`INPUT_FILE`) for the simulator to consume.
 *  4. Launch the DUT simulator (blocking execution until completion).
 *  5. Read the DUT output data from an external file (`OUTPUT_FILE`).
 *  6. Compare DUT outputs to golden reference values, reporting mismatches (up to 10 printed).
 *  7. Report overall PASS/FAIL status based on comparison results.
 *  8. Free all allocated memory and exit with the appropriate status code.
 *
 * @note The golden reference model computes y = x² for each input sample.
 *
 * @return
 *  - `0`  if all outputs match the golden reference.
 *  - `1`  if memory allocation fails.
 *  - `2`  if writing inputs to file fails.
 *  - `4`  if reading outputs from file fails or length mismatches.
 *  - `5`  if any mismatches are detected.
 */
int main(void) {
    const size_t N = 1024;

    int *in  = (int*)malloc(N * sizeof(int));
    int *gld = (int*)malloc(N * sizeof(int));
    int *out = (int*)malloc(N * sizeof(int));

    // Allocate buffers
    if (!in || !gld || !out) { fprintf(stderr, "OOM\n"); return 1; }

    // Create deterministic inputs
    for (size_t i = 0; i < N; ++i) {
        in[i]  = (int)(i - 512);       // some negatives too
        gld[i] = in[i] * in[i];        // golden: y = x*x
    }

    // 1) Emit input file
    if (write_inputs((const char *)INPUT_FILE, in, N) != 0) {
        fprintf(stderr, "ERROR: cannot write %s\n", INPUT_FILE);
        return 2;
    }

    // 2) Launch simulator (blocking)
    assert(launch_sim() == 0);

    // 3) Read DUT outputs
    if (read_outputs((const char *)OUTPUT_FILE, out, N) != 0) {
        fprintf(stderr, "ERROR: cannot read %s or length mismatch\n", OUTPUT_FILE);
        return 4;
    }

    // 4) Compare
    size_t mism = 0;
    for (size_t i = 0; i < N; ++i) {
        if (out[i] != gld[i]) {
            if (mism < 10) {
                fprintf(stderr, "MISMATCH @%zu: in=%d  out=%d  gld=%d\n",
                        i, in[i], out[i], gld[i]);
            }
            ++mism;
        }
    }

    if (mism == 0) {
        printf("[C-TB] PASS: all %zu samples matched.\n", N);
    } else {
        printf("[C-TB] FAIL: %zu mismatches out of %zu.\n", mism, N);
    }

    free(in); free(gld); free(out);
    return (mism == 0) ? 0 : 5;
}
