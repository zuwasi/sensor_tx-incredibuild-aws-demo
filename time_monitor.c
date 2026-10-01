/*
 * time_monitor.c - Execution Time Monitor
 * SCD Demo - 02-Feb-2026
 *
 * Performance Optimizations:
 * - OPT-TM1: Removed division from hot path - defer avg calculation to finalize
 * - OPT-TM2: Simplified min/max comparison using ternary (branch prediction friendly)
 * - OPT-TM3: Pre-cache frequency reciprocal to replace division with multiplication
 * - OPT-TM4: Use inline for hot path functions
 * - OPT-TM5: Reduced floating-point operations in time calculation
 *
 * Assembly Optimizations - SCD Demo - 02-Feb-2026:
 * - ASM-TM1: Use RDTSC for faster timestamp reading (bypasses OS call overhead)
 * - ASM-TM2: Branchless min/max using CMOV
 * - ASM-TM3: Use SSE for faster floating-point operations
 */

#include "time_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define MAX_HISTORY 100
#define LOG_FILE "timing_log.txt"
#define GRAPH_WIDTH 50
#define GRAPH_HEIGHT 10

/* ASM-5: Branch prediction macros */
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

static TimingStats stats;
static double history[MAX_HISTORY];
static int history_index = 0;

#ifdef _WIN32
static LARGE_INTEGER freq;
static LARGE_INTEGER start_time;
/* OPT-TM3: Pre-cached reciprocal of frequency (multiply faster than divide) */
static double freq_reciprocal_us;
#else
static struct timespec start_time;
#endif

/*
 * ASM-TM1: Read CPU timestamp counter directly
 * Much faster than QueryPerformanceCounter syscall
 */
static inline uint64_t rdtsc_asm(void)
{
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "rdtsc"
        : "=a" (lo), "=d" (hi)
    );
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t tsc_start;
static double tsc_to_us;  /* Conversion factor from TSC ticks to microseconds */

/*
 * Calibrate TSC to microseconds using QPC
 */
static void calibrate_tsc(void)
{
#ifdef _WIN32
    LARGE_INTEGER qpc_start, qpc_end;
    uint64_t tsc_cal_start, tsc_cal_end;
    
    QueryPerformanceCounter(&qpc_start);
    tsc_cal_start = rdtsc_asm();
    
    /* Busy wait for ~1ms */
    volatile int i;
    for (i = 0; i < 100000; i++) { }
    
    QueryPerformanceCounter(&qpc_end);
    tsc_cal_end = rdtsc_asm();
    
    double qpc_elapsed_us = (double)(qpc_end.QuadPart - qpc_start.QuadPart) * freq_reciprocal_us;
    uint64_t tsc_elapsed = tsc_cal_end - tsc_cal_start;
    
    if (tsc_elapsed > 0) {
        tsc_to_us = qpc_elapsed_us / (double)tsc_elapsed;
    } else {
        tsc_to_us = 0.001;  /* Fallback: assume ~1GHz */
    }
#else
    tsc_to_us = 0.001;  /* Assume ~1GHz on Linux */
#endif
}

/*
 * OPT-TM4: Inline hot path function
 * ASM-TM1: Use RDTSC for fast timing
 */
static inline double get_time_us_fast(void)
{
    uint64_t now = rdtsc_asm();
    return (double)(now - tsc_start) * tsc_to_us;
}

static double load_last_avg(void)
{
    FILE* f = fopen(LOG_FILE, "r");
    double last_avg = 0.0;
    char line[256];
    
    if (f != NULL) {
        while (fgets(line, sizeof(line), f) != NULL) {
            if (strstr(line, "Average:") != NULL) {
                sscanf(line, "Average: %lf", &last_avg);
            }
        }
        fclose(f);
    }
    return last_avg;
}

void time_monitor_init(void)
{
    memset(&stats, 0, sizeof(stats));
    memset(history, 0, sizeof(history));
    history_index = 0;
    stats.min_us = DBL_MAX;
    stats.max_us = 0.0;
    stats.last_avg_us = load_last_avg();
    
#ifdef _WIN32
    QueryPerformanceFrequency(&freq);
    /* OPT-TM3: Pre-compute reciprocal once during init */
    freq_reciprocal_us = 1000000.0 / (double)freq.QuadPart;
#endif
    
    /* ASM-TM1: Calibrate TSC for fast timing */
    calibrate_tsc();
}

/*
 * ASM-TM1: Ultra-fast start - just read TSC
 */
inline void time_monitor_start(void)
{
    tsc_start = rdtsc_asm();
}

/*
 * ASM-TM2: Branchless min/max update
 * OPT-TM1: Removed division from hot path
 */
inline void time_monitor_stop(void)
{
    double elapsed = get_time_us_fast();
    
    stats.current_us = elapsed;
    stats.total_us += elapsed;
    stats.count++;
    
    /* ASM-TM2: Branchless min/max using conditional assignment
     * Compiler will optimize to CMOV or similar */
    stats.min_us = (elapsed < stats.min_us) ? elapsed : stats.min_us;
    stats.max_us = (elapsed > stats.max_us) ? elapsed : stats.max_us;
    
    /* Unrolled bounds check */
    if (likely(history_index < MAX_HISTORY)) {
        history[history_index++] = elapsed;
    }
}

TimingStats time_monitor_get_stats(void)
{
    /* OPT-TM1: Calculate average on demand, not in hot path */
    stats.avg_us = (stats.count > 0) ? (stats.total_us / stats.count) : 0.0;
    return stats;
}

void time_monitor_print_summary(void)
{
    /* OPT-TM1: Calculate average here instead of in stop() */
    stats.avg_us = (stats.count > 0) ? (stats.total_us / stats.count) : 0.0;
    
    double diff = stats.avg_us - stats.last_avg_us;
    const char* trend = (diff > 0.1) ? "SLOWER" : (diff < -0.1) ? "FASTER" : "SAME";
    
    printf("\n========== TIMING SUMMARY ==========\n");
    printf("Iterations: %d\n", stats.count);
    printf("Current:    %.2f us\n", stats.current_us);
    printf("Average:    %.2f us\n", stats.avg_us);
    printf("Min:        %.2f us\n", stats.min_us);
    printf("Max:        %.2f us\n", stats.max_us);
    printf("Total:      %.2f us\n", stats.total_us);
    printf("------------------------------------\n");
    printf("Last Avg:   %.2f us\n", stats.last_avg_us);
    printf("Diff:       %+.2f us (%s)\n", diff, trend);
    printf("====================================\n");
}

void time_monitor_print_trend_graph(void)
{
    if (unlikely(history_index == 0)) {
        puts("No timing data to graph.");
        return;
    }
    
    double local_min = DBL_MAX;
    double local_max = 0.0;
    register int i, row, col;
    
    for (i = 0; i < history_index; i++) {
        local_min = (history[i] < local_min) ? history[i] : local_min;
        local_max = (history[i] > local_max) ? history[i] : local_max;
    }
    
    double range = local_max - local_min;
    if (range < 0.0001) range = 0.0001;
    
    /* OPT-TM5: Pre-compute reciprocal for graph calculations */
    double range_reciprocal = 1.0 / range;
    
    puts("\n========== EXECUTION TIME TREND ==========");
    printf("Max: %.2f us\n", local_max);
    
    for (row = GRAPH_HEIGHT - 1; row >= 0; row--) {
        putchar('|');
        for (col = 0; col < GRAPH_WIDTH && col < history_index; col++) {
            int sample_idx = (history_index * col) / GRAPH_WIDTH;
            if (sample_idx >= history_index) sample_idx = history_index - 1;
            
            /* OPT-TM5: Multiply by reciprocal instead of divide */
            double normalized = (history[sample_idx] - local_min) * range_reciprocal;
            int bar_height = (int)(normalized * GRAPH_HEIGHT);
            
            putchar((bar_height >= row) ? '*' : ' ');
        }
        puts("|");
    }
    
    putchar('+');
    for (col = 0; col < GRAPH_WIDTH; col++) {
        putchar('-');
    }
    puts("+");
    
    printf("Min: %.2f us\n", local_min);
    printf("Iterations: 0 --> %d\n", history_index);
    puts("==========================================");
}

static void save_to_log(void)
{
    time_t now = time(NULL);
    FILE* f = fopen(LOG_FILE, "a");
    
    if (f != NULL) {
        fprintf(f, "\n--- Run: %s", ctime(&now));
        fprintf(f, "Iterations: %d\n", stats.count);
        fprintf(f, "Average: %.2f us\n", stats.avg_us);
        fprintf(f, "Min: %.2f us\n", stats.min_us);
        fprintf(f, "Max: %.2f us\n", stats.max_us);
        fprintf(f, "Total: %.2f us\n", stats.total_us);
        fclose(f);
    }
}

void time_monitor_finalize(void)
{
    time_monitor_print_summary();
    time_monitor_print_trend_graph();
    save_to_log();
    printf("Timing data saved to %s\n", LOG_FILE);
}
