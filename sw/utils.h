#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define VSIM_CMD "vsim.exe -c -do sim/run.do"
#else
#define VSIM_CMD "vsim -c -do sim/run.do"
#endif

#define INPUT_FILE      "sim/in.dat"
#define OUTPUT_FILE     "sim/out.dat"


int write_inputs(const char *path, int *vec, size_t n) {
    if(!path || !vec || n <= 0) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (size_t i = 0; i < n; ++i) {
        fprintf(f, "%d\n", vec[i]); // decimal, one per line
    }
    fclose(f);
    return 0;
}

int read_outputs(const char *path, int *vec, size_t n) {
    if(!path || !vec || n <= 0) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    for (size_t i = 0; i < n; ++i) {
        if (fscanf(f, "%d", &vec[i]) != 1) {
            fclose(f);
            return -2; // not enough lines
        }
    }
    fclose(f);
    return 0;
}


int launch_sim(void) {
    printf("[C-TB] Launching simulator: %s\n", VSIM_CMD);
    int rc = system(VSIM_CMD);
    if (rc != 0) {
        fprintf(stderr, "ERROR: simulator returned %d\n", rc);
        return 3;
    }
    return 0;
}

#endif // __UTILS_H__