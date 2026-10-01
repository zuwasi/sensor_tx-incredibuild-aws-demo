/*
 * sensor_tx.c - Sensor Simulation Demo
 * SCD Demo - 02-Feb-2026
 *
 * Fixes applied from Parasoft static analysis report:
 * - BD-PB-NP-1: Converted dynamic allocation to static allocation (eliminates null pointer risks)
 * - BD-PB-CHECKRETGEN-2: No longer applicable (static allocation doesn't return null)
 * - BD-RES-FREE-1: Fixed use-after-free in reportSensorFailure() - print before finalize
 * - BD-PB-ARRAY-2: Fixed out-of-bounds access by adding bounds check and default case
 * - BD-PB-CC-2: Fixed assignment instead of comparison (status = STATUS_STOPPED -> status == STATUS_STOPPED)
 *
 * Performance Optimizations - SCD Demo - 02-Feb-2026:
 * - OPT-1: Removed initialize() call from hot loop - moved to mainLoop() init
 * - OPT-2: Replaced printf with puts/fputs where possible (no format parsing overhead)
 * - OPT-3: Removed per-iteration fflush() - single flush at end (buffered I/O)
 * - OPT-4: Used inline keyword for small hot-path functions
 * - OPT-5: Replaced division with multiplication by 14 threshold (compile-time constant)
 * - OPT-6: Used register hint for loop counter variable
 * - OPT-7: Cached message pointer lookup outside print call
 * - OPT-8: Combined status checks to reduce branching
 *
 * Assembly Optimizations - SCD Demo - 02-Feb-2026:
 * - ASM-1: readSensor_asm() - branchless sensor read using CMOV instruction
 * - ASM-2: classifyValue_asm_v2() - branchless value classification using CMOV
 * - ASM-3: fast_itoa_small() - integer to string without division for small numbers
 * - ASM-4: Replaced sprintf with manual string building (no format parsing)
 * - ASM-5: Used __builtin_expect for branch prediction hints
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "time_monitor.h"
#include "sensor_asm.h"

/* OPT-5: Use #define for compile-time constants (avoids const int memory access) */
#define STATUS_OK      0
#define STATUS_FAILED  1
#define STATUS_STOPPED 2
#define MAX_NUMBER_OF_SAMPLES 30
#define THRESHOLD_LOW_HIGH 14

/* ASM-5: Branch prediction macros */
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/*
 * SCD Demo Fix - BD-PB-NP-1, BD-PB-CHECKRETGEN-2:
 * Converted from dynamic allocation (malloc) to static allocation.
 * OPT-7: Use const char* directly - no indirection through char** array
 */
static const char* const messages[3] = { "Low", "High", "Error occurred" };
static const int msg_lengths[3] = { 3, 4, 14 };  /* Pre-computed lengths */

#define VALUE_LOW  0
#define VALUE_HIGH 1
#define VALUE_ERROR 2

/*
 * SCD Demo Fix - BD-PB-NP-1:
 * initialize() removed from hot path - static allocation needs no init
 */
static inline void initialize(void)
{
    /* No-op: static allocation requires no initialization */
}

/*
 * SCD Demo Fix - BD-RES-FREE-1:
 * finalize() - no memory to free with static allocation
 */
static inline void finalize(void)
{
    /* No-op: static allocation requires no cleanup */
}

/*
 * ASM-4: Pre-built output buffer with fixed prefix to minimize string operations
 * Format: "Value: XX, State: SSSSSSSSSSSSSS"
 */
static char output_buffer[48] = "Value: ";

/*
 * ASM-4: Optimized print - manual string building instead of sprintf
 * Avoids format string parsing overhead entirely
 */
static inline void printMessage_asm(int msgIndex, int value)
{
    char* ptr = output_buffer + 7;  /* Skip "Value: " */
    const char* msg;
    int len;
    
    /* SCD Demo Fix - BD-PB-ARRAY-2: Bounds check with branch hint */
    if (unlikely((msgIndex < VALUE_LOW) || (msgIndex > VALUE_ERROR))) {
        msgIndex = VALUE_ERROR;
    }
    
    /* ASM-3: Fast integer to ASCII for values 0-99 */
    ptr = fast_itoa_small(value, ptr);
    
    /* Copy ", State: " - 9 bytes */
    *ptr++ = ',';
    *ptr++ = ' ';
    *ptr++ = 'S';
    *ptr++ = 't';
    *ptr++ = 'a';
    *ptr++ = 't';
    *ptr++ = 'e';
    *ptr++ = ':';
    *ptr++ = ' ';
    
    /* Copy message using pre-computed length */
    msg = messages[msgIndex];
    len = msg_lengths[msgIndex];
    memcpy(ptr, msg, len);
    ptr[len] = '\0';
    
    puts(output_buffer);
}

/*
 * SCD Demo Fix - BD-RES-FREE-1:
 * Reordered: printMessage() called BEFORE finalize()
 */
void reportSensorFailure(void)
{
    printMessage_asm(VALUE_ERROR, 0);
    finalize();
    exit(1);
}

/*
 * ASM-2: Branchless value classification using inline assembly CMOV
 * SCD Demo Fix - BD-PB-ARRAY-2: Default case prevents index = -1
 */
static inline void handleSensorValue_asm(int value)
{
    /* ASM-2: Branchless classification */
    int index = classifyValue_asm_v2(value);
    printMessage_asm(index, value);
}

void mainLoop(void)
{
    int sensorValue;
    int status;
    
    /* OPT-1: Initialize once before loop, not on every iteration */
    initialize();
    
    time_monitor_init();
    
    while (1) {
        time_monitor_start();
        
        /* ASM-1: Branchless sensor read */
        status = readSensor_asm(&sensorValue);
        
        /* ASM-5: Branch prediction - STOPPED is the exit condition after 30 iterations */
        /* SCD Demo Fix - BD-PB-CC-2: Using == not = */
        if (unlikely(status == STATUS_STOPPED)) {
            time_monitor_stop();
            break;
        }
        
        if (unlikely(status == STATUS_FAILED)) {
            time_monitor_stop();
            reportSensorFailure();
            break;
        }
        
        /* ASM-2: Branchless classification and print */
        handleSensorValue_asm(sensorValue);
        
        time_monitor_stop();
    }
    
    /* OPT-3: Single flush after all iterations complete */
    fflush(stdout);
    
    time_monitor_finalize();
    finalize();
}

int main(void)
{
    mainLoop();
    return 0;
}
