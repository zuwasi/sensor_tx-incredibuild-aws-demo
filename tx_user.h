/*
 * tx_user.h - ThreadX User Configuration
 * SCD Training Demo - Eclipse ThreadX RTOS Integration
 * 02-Feb-2026
 */

#ifndef TX_USER_H
#define TX_USER_H

/* Define ThreadX configuration options for this application */

/* Enable performance information for all ThreadX objects */
#define TX_BLOCK_POOL_ENABLE_PERFORMANCE_INFO
#define TX_BYTE_POOL_ENABLE_PERFORMANCE_INFO
#define TX_EVENT_FLAGS_ENABLE_PERFORMANCE_INFO
#define TX_MUTEX_ENABLE_PERFORMANCE_INFO
#define TX_QUEUE_ENABLE_PERFORMANCE_INFO
#define TX_SEMAPHORE_ENABLE_PERFORMANCE_INFO
#define TX_THREAD_ENABLE_PERFORMANCE_INFO
#define TX_TIMER_ENABLE_PERFORMANCE_INFO

/* Enable stack checking for debugging */
#define TX_ENABLE_STACK_CHECKING

/* Define memory size for Win32 simulation */
#define TX_WIN32_MEMORY_SIZE                    128000

/* Bypass CPU affinity setup (can cause issues on some systems) */
#define TX_WIN32_BYPASS_AFFINITY_SETUP

/* Timer tick period in milliseconds */
#define TX_TIMER_PERIODIC                       10

/* Maximum priority levels (32-1024, divisible by 32) */
#define TX_MAX_PRIORITIES                       32

/* Minimum stack size */
#define TX_MINIMUM_STACK                        1024

#endif /* TX_USER_H */
