/*
 * sensor_threadx.c - ThreadX RTOS Sensor Application
 * SCD Training Demo - Eclipse ThreadX RTOS Integration
 * 02-Feb-2026
 *
 * ThreadX Features Demonstrated:
 * ============================================================================
 * 1. THREADS:
 *    - sensor_isr_thread: Simulates hardware interrupt (highest priority)
 *    - sensor_read_thread: Reads sensor values
 *    - data_process_thread: Processes/classifies sensor data
 *    - display_thread: Displays output to console
 *    - logger_thread: Logs data to file
 *    - stats_thread: Computes and reports statistics
 *
 * 2. COUNTING SEMAPHORES:
 *    - sensor_data_semaphore: Tracks available sensor data buffers (0-5)
 *    - processing_semaphore: Binary semaphore for processing slot
 *
 * 3. MUTEXES (with priority inheritance):
 *    - stats_mutex: Protects shared timing statistics
 *    - console_mutex: Protects console output (prevents interleaving)
 *    - log_file_mutex: Protects log file operations
 *
 * 4. MESSAGE QUEUES:
 *    - sensor_data_queue: Passes sensor readings between threads
 *    - log_queue: Passes log entries to logger thread
 *
 * 5. BYTE POOLS:
 *    - system_byte_pool: Dynamic memory for thread stacks and data
 *
 * 6. BLOCK POOLS:
 *    - sensor_block_pool: Fixed-size blocks for sensor data buffers
 *
 * 7. APPLICATION TIMERS:
 *    - sensor_sample_timer: Periodic sensor sampling trigger
 *    - stats_report_timer: Periodic statistics reporting
 *    - watchdog_timer: System watchdog (one-shot, must be kicked)
 *
 * 8. EVENT FLAGS:
 *    - system_events: Coordinate thread activities
 *
 * 9. INTERRUPT SIMULATION:
 *    - sensor_isr_thread simulates hardware interrupt behavior
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Some Eclipse CDT/MinGW indexer configurations do not expose stdio stream
 * macros while parsing, even though GCC sees them during the real build.
 * These fallbacks keep the IDE parser quiet without changing compiled output.
 */
#ifndef APP_IONBF
#define APP_IONBF (2)
#endif

#ifndef stdout
extern FILE *stdout;
#endif

#ifndef stderr
extern FILE *stderr;
#endif

#include "sensor_threadx.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#endif

/*=============================================================================
 * ThreadX Object Definitions
 *===========================================================================*/

/* Threads */
TX_THREAD               sensor_isr_thread;
TX_THREAD               sensor_read_thread;
TX_THREAD               data_process_thread;
TX_THREAD               display_thread;
TX_THREAD               logger_thread;
TX_THREAD               stats_thread;
TX_THREAD               keyboard_monitor_thread;

/* Message Queues */
TX_QUEUE                sensor_data_queue;
TX_QUEUE                log_queue;

/* Semaphores */
TX_SEMAPHORE            sensor_data_semaphore;  /* Counting semaphore */
TX_SEMAPHORE            processing_semaphore;   /* Binary semaphore */

/* Mutexes */
TX_MUTEX                stats_mutex;
TX_MUTEX                console_mutex;
TX_MUTEX                log_file_mutex;

/* Event Flags */
TX_EVENT_FLAGS_GROUP    system_events;

/* Timers */
TX_TIMER                sensor_sample_timer;
TX_TIMER                stats_report_timer;
TX_TIMER                watchdog_timer;

/* Memory Pools */
TX_BYTE_POOL            system_byte_pool;
TX_BLOCK_POOL           sensor_block_pool;

/* Statistics */
ThreadXTimingStats      timing_stats;
SystemStats             system_stats;

/* Memory area for byte pool */
static UCHAR            byte_pool_memory[BYTE_POOL_SIZE];

/* Sensor simulation state */
static volatile INT     sensor_counter = 0;
static volatile UINT    running = TX_TRUE;

/* Message strings */
static const char* const classification_messages[] = { "LOW", "HIGH", "ERROR" };

/*=============================================================================
 * High-precision Timing (Windows)
 *===========================================================================*/

#ifdef _WIN32
static LARGE_INTEGER    perf_freq;
static LARGE_INTEGER    perf_start;

static inline void timer_init(void)
{
    QueryPerformanceFrequency(&perf_freq);
}

static inline void timer_start(void)
{
    QueryPerformanceCounter(&perf_start);
}

static inline double timer_elapsed_us(void)
{
    LARGE_INTEGER perf_end;
    QueryPerformanceCounter(&perf_end);
    return ((double)(perf_end.QuadPart - perf_start.QuadPart) * 1000000.0) / (double)perf_freq.QuadPart;
}
#else
static inline void timer_init(void) {}
static inline void timer_start(void) {}
static inline double timer_elapsed_us(void) { return 0.0; }
#endif

/*=============================================================================
 * Timing Statistics Functions
 *===========================================================================*/

void timing_stats_init(void)
{
    UINT status;
    
    status = tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    if (status == TX_SUCCESS) {
        memset(&timing_stats, 0, sizeof(timing_stats));
        timing_stats.min_us = 1e9;  /* Initialize to large value */
        tx_mutex_put(&stats_mutex);
    }
    timer_init();
}

void timing_stats_record(double elapsed_us)
{
    UINT status;
    
    status = tx_mutex_get(&stats_mutex, 100);  /* 100 tick timeout */
    if (status == TX_SUCCESS) {
        timing_stats.current_us = elapsed_us;
        timing_stats.total_us += elapsed_us;
        timing_stats.count++;
        
        if (elapsed_us < timing_stats.min_us) {
            timing_stats.min_us = elapsed_us;
        }
        if (elapsed_us > timing_stats.max_us) {
            timing_stats.max_us = elapsed_us;
        }
        
        tx_mutex_put(&stats_mutex);
    } else {
        system_stats.mutex_contentions++;
    }
}

void timing_stats_print_summary(void)
{
    UINT status;
    ThreadXTimingStats local_stats;
    
    status = tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    if (status == TX_SUCCESS) {
        memcpy(&local_stats, &timing_stats, sizeof(ThreadXTimingStats));
        if (local_stats.count > 0) {
            timing_stats.avg_us = timing_stats.total_us / timing_stats.count;
        }
        local_stats = timing_stats;
        tx_mutex_put(&stats_mutex);
    } else {
        return;
    }
    
    /* Print with console mutex protection */
    status = tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
    if (status == TX_SUCCESS) {
        printf("\n========== TIMING SUMMARY (ThreadX) ==========\n");
        printf("Iterations: %lu\n", local_stats.count);
        printf("Current:    %.2f us\n", local_stats.current_us);
        printf("Average:    %.2f us\n", local_stats.avg_us);
        printf("Min:        %.2f us\n", local_stats.min_us);
        printf("Max:        %.2f us\n", local_stats.max_us);
        printf("Total:      %.2f us\n", local_stats.total_us);
        printf("==============================================\n");
        fflush(stdout);
        tx_mutex_put(&console_mutex);
    }
}

/*=============================================================================
 * ThreadX Services Summary - Print resource usage and overhead
 *===========================================================================*/

void print_threadx_services_summary(void)
{
    ULONG resumptions, suspensions, solicited_preemptions, interrupt_preemptions;
    ULONG priority_inversions, time_slices, relinquishes, timeouts;
    ULONG wait_aborts, non_idle_returns, idle_returns;
    ULONG puts, gets, suspensions_sem, timeouts_sem;
    ULONG mutex_puts, mutex_gets, mutex_suspensions, mutex_timeouts, mutex_inversions, mutex_inheritances;
    ULONG messages_sent, messages_received, empty_suspensions, full_suspensions, queue_timeouts;
    ULONG allocations, releases, fragments_searched, merges, splits, byte_suspensions, byte_timeouts;
    ULONG block_allocations, block_releases, block_suspensions, block_timeouts;
    ULONG sets, gets_ef, suspensions_ef, timeouts_ef;
    ULONG activations, reactivations, deactivations, expirations, expiration_adjusts;
    ULONG available_bytes;
    
    printf("\n");
    printf("================================================================================\n");
    printf("                     THREADX SERVICES USAGE SUMMARY                             \n");
    printf("================================================================================\n\n");
    
    /* --------------------------------------------------------------------------
     * THREADS - Core scheduling unit
     * -------------------------------------------------------------------------- */
    printf("1. THREADS (6 threads created)\n");
    printf("   WHY: Threads are the basic execution unit in ThreadX. Each handles a specific\n");
    printf("        task: ISR simulation, sensor reading, processing, display, logging, stats.\n");
    printf("   HOW: Priority-based preemptive scheduling with optional time-slicing.\n");
    printf("   +-----------------------+------------+\n");
    printf("   | Thread                | Priority   |\n");
    printf("   +-----------------------+------------+\n");
    printf("   | sensor_isr_thread     | 1 (high)   | Simulates HW interrupt\n");
    printf("   | sensor_read_thread    | 4          | Reads sensor values\n");
    printf("   | data_process_thread   | 8          | Classifies data (time-slice: 4)\n");
    printf("   | display_thread        | 12         | Console output (time-slice: 4)\n");
    printf("   | logger_thread         | 16         | File logging\n");
    printf("   | stats_thread          | 20 (low)   | Statistics reporting\n");
    printf("   +-----------------------+------------+\n");
    
    tx_thread_performance_system_info_get(&resumptions, &suspensions, &solicited_preemptions,
                                           &interrupt_preemptions, &priority_inversions,
                                           &time_slices, &relinquishes, &timeouts,
                                           &wait_aborts, &non_idle_returns, &idle_returns);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Thread resumptions:        %lu\n", resumptions);
    printf("     - Thread suspensions:        %lu\n", suspensions);
    printf("     - Preemptions (solicited):   %lu\n", solicited_preemptions);
    printf("     - Preemptions (interrupt):   %lu\n", interrupt_preemptions);
    printf("     - Priority inversions:       %lu\n", priority_inversions);
    printf("     - Time slices:               %lu\n", time_slices);
    printf("     - Thread timeouts:           %lu\n", timeouts);
    printf("     - Stack memory:              ~%d bytes (6 threads)\n", 
           TIMER_ISR_THREAD_STACK_SIZE + 2*SENSOR_THREAD_STACK_SIZE + 
           DISPLAY_THREAD_STACK_SIZE + LOGGER_THREAD_STACK_SIZE + SENSOR_THREAD_STACK_SIZE);
    
    /* --------------------------------------------------------------------------
     * SEMAPHORES - Counting and binary synchronization
     * -------------------------------------------------------------------------- */
    printf("\n2. SEMAPHORES (2 created: 1 counting, 1 binary)\n");
    printf("   WHY: Coordinate thread execution and signal data availability.\n");
    printf("   HOW: Counting semaphore tracks # of pending sensor readings.\n");
    printf("        Binary semaphore ensures exclusive processing slot.\n");
    printf("   +-------------------------+----------+----------------------------+\n");
    printf("   | Semaphore               | Type     | Purpose                    |\n");
    printf("   +-------------------------+----------+----------------------------+\n");
    printf("   | sensor_data_semaphore   | Counting | Signal data available      |\n");
    printf("   | processing_semaphore    | Binary   | Exclusive processing slot  |\n");
    printf("   +-------------------------+----------+----------------------------+\n");
    
    tx_semaphore_performance_system_info_get(&puts, &gets, &suspensions_sem, &timeouts_sem);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Semaphore puts:            %lu\n", puts);
    printf("     - Semaphore gets:            %lu\n", gets);
    printf("     - Thread suspensions:        %lu\n", suspensions_sem);
    printf("     - Wait timeouts:             %lu\n", timeouts_sem);
    printf("     - Memory per semaphore:      ~80 bytes (control block)\n");
    
    /* --------------------------------------------------------------------------
     * MUTEXES - Mutual exclusion with priority inheritance
     * -------------------------------------------------------------------------- */
    printf("\n3. MUTEXES (3 created, all with priority inheritance)\n");
    printf("   WHY: Protect shared resources from concurrent access.\n");
    printf("   HOW: Priority inheritance prevents priority inversion.\n");
    printf("   +-------------------+----------------------------------+\n");
    printf("   | Mutex             | Protects                         |\n");
    printf("   +-------------------+----------------------------------+\n");
    printf("   | stats_mutex       | Timing statistics structure      |\n");
    printf("   | console_mutex     | printf() to prevent interleaving |\n");
    printf("   | log_file_mutex    | File I/O operations              |\n");
    printf("   +-------------------+----------------------------------+\n");
    
    tx_mutex_performance_system_info_get(&mutex_puts, &mutex_gets, &mutex_suspensions,
                                          &mutex_timeouts, &mutex_inversions, &mutex_inheritances);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Mutex puts:                %lu\n", mutex_puts);
    printf("     - Mutex gets:                %lu\n", mutex_gets);
    printf("     - Thread suspensions:        %lu\n", mutex_suspensions);
    printf("     - Wait timeouts:             %lu\n", mutex_timeouts);
    printf("     - Priority inversions:       %lu\n", mutex_inversions);
    printf("     - Memory per mutex:          ~100 bytes (control block)\n");
    
    /* --------------------------------------------------------------------------
     * MESSAGE QUEUES - Inter-thread communication
     * -------------------------------------------------------------------------- */
    printf("\n4. MESSAGE QUEUES (2 created)\n");
    printf("   WHY: Decouple producers from consumers, buffer data between threads.\n");
    printf("   HOW: FIFO queues pass structured messages (SensorMessage, LogEntry).\n");
    printf("   +-------------------+-------+---------------------------+\n");
    printf("   | Queue             | Depth | Message Size              |\n");
    printf("   +-------------------+-------+---------------------------+\n");
    printf("   | sensor_data_queue | 32    | %lu bytes (SensorMessage) |\n", (ULONG)sizeof(SensorMessage));
    printf("   | log_queue         | 64    | %lu bytes (LogEntry)      |\n", (ULONG)sizeof(LogEntry));
    printf("   +-------------------+-------+---------------------------+\n");
    
    tx_queue_performance_system_info_get(&messages_sent, &messages_received, 
                                          &empty_suspensions, &full_suspensions, 
                                          &full_suspensions, &queue_timeouts);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Messages sent:             %lu\n", messages_sent);
    printf("     - Messages received:         %lu\n", messages_received);
    printf("     - Empty suspensions:         %lu\n", empty_suspensions);
    printf("     - Full suspensions:          %lu\n", full_suspensions);
    printf("     - Queue timeouts:            %lu\n", queue_timeouts);
    printf("     - Queue memory:              ~%lu bytes\n", 
           (ULONG)(SENSOR_QUEUE_DEPTH * sizeof(SensorMessage) + LOG_QUEUE_DEPTH * sizeof(LogEntry)));
    
    /* --------------------------------------------------------------------------
     * BYTE POOL - Variable-size dynamic memory
     * -------------------------------------------------------------------------- */
    printf("\n5. BYTE POOL (1 created: %d bytes)\n", BYTE_POOL_SIZE);
    printf("   WHY: Dynamic memory allocation for thread stacks and data structures.\n");
    printf("   HOW: First-fit algorithm with fragmentation management.\n");
    
    tx_byte_pool_performance_system_info_get(&allocations, &releases, &fragments_searched,
                                              &merges, &splits, &byte_suspensions, &byte_timeouts);
    tx_byte_pool_info_get(&system_byte_pool, TX_NULL, &available_bytes, TX_NULL, 
                          TX_NULL, TX_NULL, TX_NULL);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Allocations:               %lu\n", allocations);
    printf("     - Releases:                  %lu\n", releases);
    printf("     - Fragments searched:        %lu\n", fragments_searched);
    printf("     - Merges:                    %lu\n", merges);
    printf("     - Splits:                    %lu\n", splits);
    printf("     - Available bytes:           %lu of %d (%.1f%% used)\n", 
           available_bytes, BYTE_POOL_SIZE, 
           100.0 * (1.0 - (double)available_bytes / BYTE_POOL_SIZE));
    
    /* --------------------------------------------------------------------------
     * BLOCK POOL - Fixed-size memory blocks
     * -------------------------------------------------------------------------- */
    printf("\n6. BLOCK POOL (1 created: %d blocks x %d bytes)\n", BLOCK_COUNT, BLOCK_SIZE);
    printf("   WHY: Fast, deterministic allocation for fixed-size sensor buffers.\n");
    printf("   HOW: Pre-allocated blocks with O(1) alloc/free, no fragmentation.\n");
    
    tx_block_pool_performance_system_info_get(&block_allocations, &block_releases, &block_suspensions, &block_timeouts);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Block allocations:         %lu\n", block_allocations);
    printf("     - Block releases:            %lu\n", block_releases);
    printf("     - Thread suspensions:        %lu\n", block_suspensions);
    printf("     - Pool memory:               %d bytes\n", BLOCK_COUNT * BLOCK_SIZE);
    
    /* --------------------------------------------------------------------------
     * EVENT FLAGS - Multi-bit event signaling
     * -------------------------------------------------------------------------- */
    printf("\n7. EVENT FLAGS (1 group created with 8 flags)\n");
    printf("   WHY: Coordinate multiple events across threads with single wait.\n");
    printf("   HOW: Bit-field allows OR/AND waiting on multiple conditions.\n");
    printf("   +-----------------------------+------+--------------------------+\n");
    printf("   | Event Flag                  | Bit  | Purpose                  |\n");
    printf("   +-----------------------------+------+--------------------------+\n");
    printf("   | SENSOR_EVENT_TIMER_TICK            | 0x01 | Timer fired              |\n");
    printf("   | SENSOR_EVENT_SENSOR_DATA_READY     | 0x02 | Data available           |\n");
    printf("   | SENSOR_EVENT_PROCESSING_COMPLETE   | 0x04 | Processing done          |\n");
    printf("   | SENSOR_EVENT_DISPLAY_UPDATE        | 0x08 | Update display           |\n");
    printf("   | SENSOR_EVENT_ERROR_OCCURRED        | 0x10 | Error condition          |\n");
    printf("   | SENSOR_EVENT_SYSTEM_SHUTDOWN       | 0x20 | Shutdown signal          |\n");
    printf("   | APP_EVENT_LOG_FLUSH             | 0x40 | Flush log                |\n");
    printf("   | SENSOR_EVENT_WATCHDOG_KICK         | 0x80 | Watchdog alive           |\n");
    printf("   +-----------------------------+------+--------------------------+\n");
    
    tx_event_flags_performance_system_info_get(&sets, &gets_ef, &suspensions_ef, &timeouts_ef);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Flag sets:                 %lu\n", sets);
    printf("     - Flag gets:                 %lu\n", gets_ef);
    printf("     - Thread suspensions:        %lu\n", suspensions_ef);
    printf("     - Wait timeouts:             %lu\n", timeouts_ef);
    printf("     - Memory per group:          ~48 bytes (control block)\n");
    
    /* --------------------------------------------------------------------------
     * APPLICATION TIMERS - Periodic and one-shot callbacks
     * -------------------------------------------------------------------------- */
    printf("\n8. APPLICATION TIMERS (3 created)\n");
    printf("   WHY: Trigger periodic actions and timeout detection.\n");
    printf("   HOW: Timer callbacks execute in timer thread context.\n");
    printf("   +----------------------+----------+--------+------------------+\n");
    printf("   | Timer                | Type     | Period | Purpose          |\n");
    printf("   +----------------------+----------+--------+------------------+\n");
    printf("   | sensor_sample_timer  | Periodic | 10 tk  | Trigger sampling |\n");
    printf("   | stats_report_timer   | Periodic | 100 tk | Stats display    |\n");
    printf("   | watchdog_timer       | One-shot | 500 tk | Hang detection   |\n");
    printf("   +----------------------+----------+--------+------------------+\n");
    
    tx_timer_performance_system_info_get(&activations, &reactivations, &deactivations,
                                          &expirations, &expiration_adjusts);
    
    printf("   OVERHEAD/USAGE:\n");
    printf("     - Timer activations:         %lu\n", activations);
    printf("     - Timer reactivations:       %lu\n", reactivations);
    printf("     - Timer deactivations:       %lu\n", deactivations);
    printf("     - Timer expirations:         %lu\n", expirations);
    printf("     - Memory per timer:          ~40 bytes (control block)\n");
    
    /* --------------------------------------------------------------------------
     * OVERALL RESOURCE SUMMARY
     * -------------------------------------------------------------------------- */
    printf("\n================================================================================\n");
    printf("                          RESOURCE OVERHEAD SUMMARY                             \n");
    printf("================================================================================\n");
    printf("   ThreadX Control Blocks:\n");
    printf("     - 6 Threads:        ~%d bytes (TX_THREAD * 6)\n", 6 * 200);
    printf("     - 2 Semaphores:     ~160 bytes\n");
    printf("     - 3 Mutexes:        ~300 bytes\n");
    printf("     - 2 Queues:         ~160 bytes\n");
    printf("     - 1 Byte Pool:      ~80 bytes\n");
    printf("     - 1 Block Pool:     ~48 bytes\n");
    printf("     - 1 Event Group:    ~48 bytes\n");
    printf("     - 3 Timers:         ~120 bytes\n");
    printf("     ----------------------------------------\n");
    printf("     Total Control:      ~%d bytes\n", 6*200 + 160 + 300 + 160 + 80 + 48 + 48 + 120);
    printf("\n");
    printf("   Memory Pools:\n");
    printf("     - Byte Pool:        %d bytes\n", BYTE_POOL_SIZE);
    printf("     - Block Pool:       %d bytes\n", BLOCK_COUNT * BLOCK_SIZE);
    printf("     - Queue Buffers:    ~%lu bytes\n", 
           (ULONG)(SENSOR_QUEUE_DEPTH * sizeof(SensorMessage) + LOG_QUEUE_DEPTH * sizeof(LogEntry)));
    printf("\n");
    printf("   Application Statistics:\n");
    printf("     - Total readings:   %lu\n", system_stats.total_readings);
    printf("     - Timer fires:      %lu\n", system_stats.timer_expirations);
    printf("     - Queue overflows:  %lu\n", system_stats.queue_overflows);
    printf("     - Mutex contentions:%lu\n", system_stats.mutex_contentions);
    printf("     - Sem timeouts:     %lu\n", system_stats.semaphore_timeouts);
    printf("================================================================================\n");
    fflush(stdout);
}

/*=============================================================================
 * Sensor Functions
 *===========================================================================*/

/* Read sensor value (randomized simulation) */
INT read_sensor(INT *value)
{
    INT random_state;

    if (sensor_counter < 40) {
        random_state = rand() % 3 + 1;
    } else {
        random_state = rand() % 4 + 1;
    }
    sensor_counter++;

    switch (random_state) {
        case 1:
            *value = rand() % 15;
            return SENSOR_STATUS_OK;
        case 2:
            *value = 15 + rand() % 15;
            return SENSOR_STATUS_OK;
        case 3:
            *value = 0;
            return SENSOR_STATUS_ERROR;
        case 4:
            *value = 0;
            return SENSOR_STATUS_SELF_CHECK;
        default:
            *value = 0;
            return SENSOR_STATUS_ERROR;
    }
}

/* Classify sensor value */
INT classify_value(INT value)
{
    if (value >= MAX_SENSOR_VALUE) {
        return VALUE_ERROR;
    }
    return (value > THRESHOLD_LOW_HIGH) ? VALUE_HIGH : VALUE_LOW;
}

/* Print message with mutex protection */
void print_message(INT classification, INT value)
{
    UINT status;
    
    if (classification < 0 || classification > VALUE_ERROR) {
        classification = VALUE_ERROR;
    }
    
    status = tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
    if (status == TX_SUCCESS) {
        printf("Value: %d, State: %s\n", value, classification_messages[classification]);
        tx_mutex_put(&console_mutex);
    }
}

/* Log message to queue */
void log_message(UINT severity, const char *message)
{
    LogEntry entry;
    UINT status;

    entry.timestamp = tx_time_get();
    entry.severity = severity;
    if (message != NULL) {
        /* AMP fixed: use snprintf to guarantee null-termination and avoid overflow. */
        (void)snprintf(entry.message, sizeof(entry.message), "%s", message);
    } else {
        entry.message[0] = '\0';
    }
    
    /* Try to send, don't wait if queue is full */
    status = tx_queue_send(&log_queue, &entry, TX_NO_WAIT);
    if (status == TX_QUEUE_FULL) {
        system_stats.queue_overflows++;
    }
}

/*=============================================================================
 * Thread Entry Functions
 *===========================================================================*/

/*
 * Sensor ISR Thread - Simulates hardware interrupt
 * Highest priority thread that triggers sensor sampling
 */
void sensor_isr_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);
    
    log_message(0, "Sensor ISR thread started");
    
    while (running) {
        /* Wait for timer event (simulating interrupt) */
        ULONG actual_events;
        UINT status = tx_event_flags_get(&system_events, 
        								PERF_TIMER_TICK,
                                         TX_OR_CLEAR,
                                         &actual_events,
                                         TX_WAIT_FOREVER);
        
        if (status == TX_SUCCESS) {
            /* Signal that sensor data is available via counting semaphore */
            tx_semaphore_put(&sensor_data_semaphore);
            
            /* Also set event flag for threads waiting on events */
            tx_event_flags_set(&system_events, SENSOR_EVENT_SENSOR_DATA_READY, TX_OR);
        }
        
        /* Check for shutdown */
        tx_event_flags_get(&system_events, SENSOR_EVENT_SYSTEM_SHUTDOWN,
                          TX_OR, &actual_events, TX_NO_WAIT);
        if (actual_events & SENSOR_EVENT_SYSTEM_SHUTDOWN) {
            break;
        }
    }
    
    log_message(0, "Sensor ISR thread exiting");
}

/*
 * Sensor Read Thread - Reads sensor values
 * Uses counting semaphore to wait for available data
 */
void sensor_read_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);
    
    INT sensor_value;
    INT status;
    SensorMessage msg;
    static UINT sequence = 0;
    
    log_message(0, "Sensor read thread started");
    
    while (running) {
        /* Wait on counting semaphore for data availability */
        UINT sem_status = tx_semaphore_get(&sensor_data_semaphore, 50);
        
        if (sem_status == TX_SUCCESS) {
            timer_start();
            
            /* Read sensor */
            status = read_sensor(&sensor_value);
            
            if (status == SENSOR_STATUS_ERROR) {
                log_message(2, "Sensor failure detected!");
                tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
                printf("[SENSOR] ERROR: Sensor failure detected!\n");
                tx_mutex_put(&console_mutex);
                system_stats.total_readings++;
                tx_event_flags_set(&system_events, SENSOR_EVENT_WATCHDOG_KICK, TX_OR);
                continue;
            }
            
            if (status == SENSOR_STATUS_SELF_CHECK) {
                tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
                printf(">>> SYSTEM SELF CHECK <<<\n");
                tx_mutex_put(&console_mutex);
                log_message(1, "System self-check triggered");
                tx_thread_sleep(500);
                perform_self_check();
                system_stats.total_readings++;
                tx_event_flags_set(&system_events, SENSOR_EVENT_WATCHDOG_KICK, TX_OR);
                continue;
            }
            
            /* Prepare message for queue */
            msg.timestamp = tx_time_get();
            msg.sensor_value = sensor_value;
            msg.classification = classify_value(sensor_value);
            msg.sequence_number = sequence++;
            
            /* Send to processing queue */
            UINT queue_status = tx_queue_send(&sensor_data_queue, &msg, 100);
            if (queue_status != TX_SUCCESS) {
                system_stats.queue_overflows++;
            }
            
            double elapsed = timer_elapsed_us();
            timing_stats_record(elapsed);
            
            system_stats.total_readings++;
            
            /* Kick watchdog */
            tx_event_flags_set(&system_events, SENSOR_EVENT_WATCHDOG_KICK, TX_OR);
            
        } else if (sem_status == TX_NO_INSTANCE) {
            system_stats.semaphore_timeouts++;
        }
        
        /* Check for shutdown */
        ULONG actual_events;
        tx_event_flags_get(&system_events, SENSOR_EVENT_SYSTEM_SHUTDOWN,
                          TX_OR, &actual_events, TX_NO_WAIT);
        if (actual_events & SENSOR_EVENT_SYSTEM_SHUTDOWN) {
            break;
        }
    }
    
    log_message(0, "Sensor read thread exiting");
}

/*
 * Data Processing Thread - Processes sensor data
 * Uses message queue and binary semaphore for processing slot
 */
void data_process_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);
    
    SensorMessage msg;
    
    log_message(0, "Data process thread started");
    
    while (running) {
        /* Receive message from queue */
        UINT status = tx_queue_receive(&sensor_data_queue, &msg, 100);
        
        if (status == TX_SUCCESS) {
            /* Get processing slot (binary semaphore) */
            tx_semaphore_get(&processing_semaphore, TX_WAIT_FOREVER);
            
            /* Update statistics based on classification */
            switch (msg.classification) {
                case VALUE_LOW:
                    system_stats.low_count++;
                    break;
                case VALUE_HIGH:
                    system_stats.high_count++;
                    break;
                case VALUE_ERROR:
                    system_stats.error_count++;
                    tx_event_flags_set(&system_events, SENSOR_EVENT_ERROR_OCCURRED, TX_OR);
                    break;
            }
            
            /* Signal display thread */
            tx_event_flags_set(&system_events, SENSOR_EVENT_PROCESSING_COMPLETE, TX_OR);
            
            /* Display the result */
            print_message(msg.classification, msg.sensor_value);
            
            /* Release processing slot */
            tx_semaphore_put(&processing_semaphore);
        }
        
        /* Check for shutdown */
        ULONG actual_events;
        tx_event_flags_get(&system_events, SENSOR_EVENT_SYSTEM_SHUTDOWN,
                          TX_OR, &actual_events, TX_NO_WAIT);
        if (actual_events & SENSOR_EVENT_SYSTEM_SHUTDOWN) {
            break;
        }
    }
    
    log_message(0, "Data process thread exiting");
}

/*
 * Display Thread - Updates display
 * Uses event flags for coordination
 */
void display_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);
    
    ULONG actual_events;
    ULONG display_count = 0;
    
    log_message(0, "Display thread started");
    
    while (running) {
        /* Wait for display update event */
        UINT status = tx_event_flags_get(&system_events,
                                         SENSOR_EVENT_PROCESSING_COMPLETE | SENSOR_EVENT_SYSTEM_SHUTDOWN,
                                         TX_OR_CLEAR,
                                         &actual_events,
                                         100);
        
        if (status == TX_SUCCESS) {
            if (actual_events & SENSOR_EVENT_SYSTEM_SHUTDOWN) {
                break;
            }
            
            if (actual_events & SENSOR_EVENT_PROCESSING_COMPLETE) {
                display_count++;
                
                /* Every 10 updates, signal display refresh */
                if ((display_count % 10) == 0) {
                    tx_event_flags_set(&system_events, SENSOR_EVENT_DISPLAY_UPDATE, TX_OR);
                }
            }
        }
    }
    
    log_message(0, "Display thread exiting");
}

/*
 * Logger Thread - Logs messages to file
 * Uses message queue and file mutex
 */
void logger_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);
    
    LogEntry entry;
    FILE *log_file = NULL;
    ULONG messages_logged = 0;
    
    log_message(0, "Logger thread started");
    
    /* Open log file with mutex protection */
    tx_mutex_get(&log_file_mutex, TX_WAIT_FOREVER);
    log_file = fopen("threadx_sensor_log.txt", "w");
    if (log_file) {
        fprintf(log_file, "=== ThreadX Sensor Log ===\n");
        fflush(log_file);
    }
    tx_mutex_put(&log_file_mutex);
    
    while (running) {
        /* Receive log entry from queue */
        UINT status = tx_queue_receive(&log_queue, &entry, 100);
        
        if (status == TX_SUCCESS) {
            if (log_file) {
                tx_mutex_get(&log_file_mutex, TX_WAIT_FOREVER);
                
                const char *severity_str = (entry.severity == 0) ? "INFO" :
                                          (entry.severity == 1) ? "WARN" : "ERROR";
                fprintf(log_file, "[%lu] %s: %s\n", 
                        entry.timestamp, severity_str, entry.message);
                
                messages_logged++;
                
                /* Flush every 10 messages */
                if ((messages_logged % 10) == 0) {
                    fflush(log_file);
                }
                
                tx_mutex_put(&log_file_mutex);
            }
        }
        
        /* Check for shutdown or flush request */
        ULONG actual_events;
        tx_event_flags_get(&system_events, SENSOR_EVENT_SYSTEM_SHUTDOWN | APP_EVENT_LOG_FLUSH,
                          TX_OR, &actual_events, TX_NO_WAIT);
        
        if (actual_events & APP_EVENT_LOG_FLUSH) {
            tx_event_flags_set(&system_events, ~APP_EVENT_LOG_FLUSH, TX_AND);
            if (log_file) {
                tx_mutex_get(&log_file_mutex, TX_WAIT_FOREVER);
                fflush(log_file);
                tx_mutex_put(&log_file_mutex);
            }
        }
        
        if (actual_events & SENSOR_EVENT_SYSTEM_SHUTDOWN) {
            break;
        }
    }
    
    /* Close log file */
    if (log_file) {
        tx_mutex_get(&log_file_mutex, TX_WAIT_FOREVER);
        fprintf(log_file, "\n=== Log End: %lu messages ===\n", messages_logged);
        fclose(log_file);
        tx_mutex_put(&log_file_mutex);
    }
    
    /* Use console mutex for final message */
    tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
    printf("Logger thread: wrote %lu messages to threadx_sensor_log.txt\n", messages_logged);
    tx_mutex_put(&console_mutex);
}

/*
 * Statistics Thread - Computes and reports statistics
 * Background priority, periodic reporting
 */
void stats_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);
    
    ULONG report_count = 0;
    
    log_message(0, "Statistics thread started");
    
    while (running) {
        /* Sleep for a period before reporting */
        tx_thread_sleep(50);
        
        report_count++;
        
        /* Every few cycles, print a mini-report */
        if ((report_count % 4) == 0) {
            UINT status = tx_mutex_get(&console_mutex, 20);
            if (status == TX_SUCCESS) {
                printf("  [Stats] Readings: %lu (L:%lu H:%lu E:%lu)\n",
                       system_stats.total_readings,
                       system_stats.low_count,
                       system_stats.high_count,
                       system_stats.error_count);
                tx_mutex_put(&console_mutex);
            }
        }
        
        /* Check for shutdown */
        ULONG actual_events;
        tx_event_flags_get(&system_events, SENSOR_EVENT_SYSTEM_SHUTDOWN,
                          TX_OR, &actual_events, TX_NO_WAIT);
        if (actual_events & SENSOR_EVENT_SYSTEM_SHUTDOWN) {
            break;
        }
    }
    
    log_message(0, "Statistics thread exiting");
}

/*=============================================================================
 * Self-Check Function (Intentional buffer overrun for Parasoft demo)
 *===========================================================================*/

void perform_self_check(void)
{
    char check_buffer[16];
    const char *test_message = "SELF-CHECK-PASS: System diagnostics completed successfully";
    
    tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
    printf("[SELF CHECK] Running system diagnostics...\n");
    printf("[SELF CHECK] Checking memory integrity...\n");
    printf("[SELF CHECK] Checking sensor calibration...\n");
    tx_mutex_put(&console_mutex);
    
    /* BUG 1: strcpy overrun (58 bytes into 16-byte buffer) */
    strcpy(check_buffer, test_message);
    
    tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
    printf("[SELF CHECK] Result: %s\n", check_buffer);
    tx_mutex_put(&console_mutex);
    
    /* BUG 2: Aggressive memset overrun (2048 bytes into 16-byte buffer) to guarantee crash */
    memset(check_buffer, 0x41, 2048);
    
    tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
    printf("[SELF CHECK] Verification complete: %s\n", check_buffer);
    tx_mutex_put(&console_mutex);
}

/*=============================================================================
 * Keyboard Monitor Thread
 *===========================================================================*/

void keyboard_monitor_thread_entry(ULONG thread_input)
{
    TX_PARAMETER_NOT_USED(thread_input);

    log_message(0, "Keyboard monitor thread started");

    while (running) {
#ifdef _WIN32
        if (_kbhit()) {
        	 int16_t ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                running = TX_FALSE;
                tx_event_flags_set(&system_events, SENSOR_EVENT_SYSTEM_SHUTDOWN, TX_OR);

                tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);
                printf("User pressed 'q' - shutting down...\n");
                tx_mutex_put(&console_mutex);

                tx_thread_sleep(20);
                print_threadx_services_summary();
                break;
            }
        }
#endif
        tx_thread_sleep(5);
    }

    log_message(0, "Keyboard monitor thread exiting");
}

/*=============================================================================
 * Timer Callback Functions
 *===========================================================================*/

/*
 * Sensor Sample Timer Callback
 * Periodic timer that triggers sensor sampling
 */
void sensor_sample_timer_callback(ULONG timer_input)
{
    TX_PARAMETER_NOT_USED(timer_input);
    
    system_stats.timer_expirations++;
    
    /* Set event to trigger sensor ISR thread */
    tx_event_flags_set(&system_events, SENSOR_EVENT_TIMER_TICK, TX_OR);
}

/*
 * Statistics Report Timer Callback
 * Periodic timer for statistics reporting
 */
void stats_report_timer_callback(ULONG timer_input)
{
    TX_PARAMETER_NOT_USED(timer_input);
    
    /* Signal display update */
    tx_event_flags_set(&system_events, SENSOR_EVENT_DISPLAY_UPDATE, TX_OR);
}

/*
 * Watchdog Timer Callback
 * One-shot timer - if not kicked, system may have hung
 */
void watchdog_timer_callback(ULONG timer_input)
{
    TX_PARAMETER_NOT_USED(timer_input);
    
    /* Check if watchdog was kicked */
    ULONG actual_events;
    UINT status = tx_event_flags_get(&system_events, SENSOR_EVENT_WATCHDOG_KICK,
                                     TX_OR_CLEAR, &actual_events, TX_NO_WAIT);
    
    if (status == TX_SUCCESS && (actual_events & SENSOR_EVENT_WATCHDOG_KICK)) {
        /* Watchdog was kicked, restart timer */
        tx_timer_change(&watchdog_timer, WATCHDOG_TIMEOUT, 0);
        tx_timer_activate(&watchdog_timer);
    } else {
        /* Watchdog timeout - system may be hung */
        log_message(2, "WATCHDOG TIMEOUT - System may be hung!");
    }
}

/*=============================================================================
 * Application Entry Point
 *===========================================================================*/

/* Define application entry point - called by tx_kernel_enter() */

void tx_application_define(void *first_unused_memory)
{
    CHAR *pointer = TX_NULL;
    UINT status;
    
    TX_PARAMETER_NOT_USED(first_unused_memory);
    
    srand((uint32_t)time(NULL));

    printf("==============================================\n");
    printf("  Eclipse ThreadX Sensor Demo Application\n");
    printf("  SCD Training Demo - 02-Feb-2026\n");
    printf("  Press 'q' to quit at any time.\n");
    printf("==============================================\n\n");
    fflush(stdout);
    
    /* Initialize statistics */
    memset(&timing_stats, 0, sizeof(timing_stats));
    memset(&system_stats, 0, sizeof(system_stats));
    timing_stats.min_us = 1e9;
    
    /*=========================================================================
     * Create Byte Pool for dynamic memory allocation
     *=======================================================================*/
    status = tx_byte_pool_create(&system_byte_pool, "System Byte Pool",
                                  byte_pool_memory, BYTE_POOL_SIZE);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create byte pool\n");
        return;
    }
    printf("[INIT] Byte pool created (%d bytes)\n", BYTE_POOL_SIZE);
    
    /*=========================================================================
     * Create Block Pool for fixed-size sensor buffers
     *=======================================================================*/
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer, 
                     BLOCK_SIZE * BLOCK_COUNT, TX_NO_WAIT);
    status = tx_block_pool_create(&sensor_block_pool, "Sensor Block Pool",
                                    BLOCK_SIZE, pointer, BLOCK_SIZE * BLOCK_COUNT);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create block pool\n");
        return;
    }
    printf("[INIT] Block pool created (%d blocks x %d bytes)\n", BLOCK_COUNT, BLOCK_SIZE);
    
    /*=========================================================================
     * Create Message Queues
     *=======================================================================*/
    
    /* Sensor data queue */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     SENSOR_QUEUE_DEPTH * sizeof(SensorMessage), TX_NO_WAIT);
    status = tx_queue_create(&sensor_data_queue, "Sensor Data Queue",
                              sizeof(SensorMessage) / sizeof(ULONG),
                              pointer, SENSOR_QUEUE_DEPTH * sizeof(SensorMessage));
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create sensor queue\n");
        return;
    }
    printf("[INIT] Sensor data queue created (depth: %d)\n", SENSOR_QUEUE_DEPTH);
    
    /* Log queue */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     LOG_QUEUE_DEPTH * sizeof(LogEntry), TX_NO_WAIT);
    status = tx_queue_create(&log_queue, "Log Queue",
                              sizeof(LogEntry) / sizeof(ULONG),
                              pointer, LOG_QUEUE_DEPTH * sizeof(LogEntry));
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create log queue\n");
        return;
    }
    printf("[INIT] Log queue created (depth: %d)\n", LOG_QUEUE_DEPTH);
    
    /*=========================================================================
     * Create Counting Semaphores
     *=======================================================================*/
    
    /* Counting semaphore for sensor data buffers (starts at 0) */
    status = tx_semaphore_create(&sensor_data_semaphore, "Sensor Data Semaphore",
                                   SENSOR_DATA_SEM_INITIAL);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create sensor semaphore\n");
        return;
    }
    printf("[INIT] Sensor data counting semaphore created (initial: %d)\n", 
           SENSOR_DATA_SEM_INITIAL);
    
    /* Binary semaphore for processing slot (starts at 1) */
    status = tx_semaphore_create(&processing_semaphore, "Processing Semaphore", 1);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create processing semaphore\n");
        return;
    }
    printf("[INIT] Processing binary semaphore created\n");
    
    /*=========================================================================
     * Create Mutexes (with priority inheritance)
     *=======================================================================*/
    
    status = tx_mutex_create(&stats_mutex, "Stats Mutex", TX_INHERIT);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create stats mutex\n");
        return;
    }
    
    status = tx_mutex_create(&console_mutex, "Console Mutex", TX_INHERIT);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create console mutex\n");
        return;
    }
    
    status = tx_mutex_create(&log_file_mutex, "Log File Mutex", TX_INHERIT);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create log file mutex\n");
        return;
    }
    printf("[INIT] Mutexes created (with priority inheritance)\n");
    
    /*=========================================================================
     * Create Event Flags Group
     *=======================================================================*/
    
    status = tx_event_flags_create(&system_events, "System Events");
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create event flags\n");
        return;
    }
    printf("[INIT] Event flags group created\n");
    
    /*=========================================================================
     * Create Threads
     *=======================================================================*/
    
    /* Sensor ISR Thread (highest priority - simulates interrupt) */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer, 
                     TIMER_ISR_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&sensor_isr_thread, "Sensor ISR Thread",
                               sensor_isr_thread_entry, 0,
                               pointer, TIMER_ISR_THREAD_STACK_SIZE,
                               SENSOR_ISR_PRIORITY, SENSOR_ISR_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create sensor ISR thread\n");
        return;
    }
    
    /* Sensor Read Thread */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     SENSOR_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&sensor_read_thread, "Sensor Read Thread",
                               sensor_read_thread_entry, 0,
                               pointer, SENSOR_THREAD_STACK_SIZE,
                               SENSOR_READ_PRIORITY, SENSOR_READ_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create sensor read thread\n");
        return;
    }
    
    /* Data Processing Thread */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     SENSOR_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&data_process_thread, "Data Process Thread",
                               data_process_thread_entry, 0,
                               pointer, SENSOR_THREAD_STACK_SIZE,
                               DATA_PROCESS_PRIORITY, DATA_PROCESS_PRIORITY,
                               4, TX_AUTO_START);  /* Time slice of 4 ticks */
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create data process thread\n");
        return;
    }
    
    /* Display Thread */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     DISPLAY_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&display_thread, "Display Thread",
                               display_thread_entry, 0,
                               pointer, DISPLAY_THREAD_STACK_SIZE,
                               DISPLAY_PRIORITY, DISPLAY_PRIORITY,
                               4, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create display thread\n");
        return;
    }
    
    /* Logger Thread */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     LOGGER_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&logger_thread, "Logger Thread",
                               logger_thread_entry, 0,
                               pointer, LOGGER_THREAD_STACK_SIZE,
                               LOGGER_PRIORITY, LOGGER_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create logger thread\n");
        return;
    }
    
    /* Statistics Thread (lowest priority) */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     SENSOR_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&stats_thread, "Stats Thread",
                               stats_thread_entry, 0,
                               pointer, SENSOR_THREAD_STACK_SIZE,
                               STATS_PRIORITY, STATS_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create stats thread\n");
        return;
    }
    
    /* Keyboard Monitor Thread */
    tx_byte_allocate(&system_byte_pool, (VOID **)&pointer,
                     KEYBOARD_THREAD_STACK_SIZE, TX_NO_WAIT);
    status = tx_thread_create(&keyboard_monitor_thread, "Keyboard Monitor Thread",
                               keyboard_monitor_thread_entry, 0,
                               pointer, KEYBOARD_THREAD_STACK_SIZE,
                               KEYBOARD_MONITOR_PRIORITY, KEYBOARD_MONITOR_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create keyboard monitor thread\n");
        return;
    }
    
    printf("[INIT] All threads created:\n");
    printf("       - Sensor ISR (priority %d)\n", SENSOR_ISR_PRIORITY);
    printf("       - Keyboard Monitor (priority %d)\n", KEYBOARD_MONITOR_PRIORITY);
    printf("       - Sensor Read (priority %d)\n", SENSOR_READ_PRIORITY);
    printf("       - Data Process (priority %d, time-slice: 4)\n", DATA_PROCESS_PRIORITY);
    printf("       - Display (priority %d, time-slice: 4)\n", DISPLAY_PRIORITY);
    printf("       - Logger (priority %d)\n", LOGGER_PRIORITY);
    printf("       - Stats (priority %d)\n", STATS_PRIORITY);
    
    /*=========================================================================
     * Create Application Timers
     *=======================================================================*/
    
    /* Sensor sampling timer (periodic) */
    status = tx_timer_create(&sensor_sample_timer, "Sensor Sample Timer",
                              sensor_sample_timer_callback, 0,
                              SENSOR_SAMPLE_PERIOD,   /* Initial ticks */
                              SENSOR_SAMPLE_PERIOD,   /* Reschedule ticks */
                              TX_AUTO_ACTIVATE);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create sensor timer\n");
        return;
    }
    printf("[INIT] Sensor sample timer created (period: %d ticks)\n", SENSOR_SAMPLE_PERIOD);
    
    /* Statistics report timer (periodic) */
    status = tx_timer_create(&stats_report_timer, "Stats Report Timer",
                              stats_report_timer_callback, 0,
                              STATS_REPORT_PERIOD,
                              STATS_REPORT_PERIOD,
                              TX_AUTO_ACTIVATE);
    if (status != TX_SUCCESS) {
        printf("ERROR: Failed to create stats timer\n");

        return;
    }
    printf("[INIT] Stats report timer created (period: %d ticks)\n", STATS_REPORT_PERIOD);
    
    printf("\n[INIT] System initialization complete!\n");
    printf("       Starting ThreadX scheduler...\n\n");
}

/*=============================================================================
 * Intentional Parasoft Demo Bugs
 *===========================================================================*/

/*
 * These functions are intentionally defective and are not called by the demo.
 * They exist only to validate Parasoft findings, especially ThreadX-aware
 * multithreading rules.
 */

volatile ULONG parasoft_demo_shared_counter = 0;

void parasoft_demo_race_condition(void)
{
    /* BUG: Unsynchronized access to shared state. */
    parasoft_demo_shared_counter++;
}

void parasoft_demo_double_lock_deadlock(void)
{
    /* BUG: Recursive lock attempt on the same mutex can deadlock. */
    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    tx_mutex_put(&stats_mutex);
}

void parasoft_demo_missing_unlock(UINT fail)
{
    tx_mutex_get(&console_mutex, TX_WAIT_FOREVER);

    if (fail != 0U) {
        /* BUG: Early return while console_mutex is still locked. */
        return;
    }

    tx_mutex_put(&console_mutex);
}

void parasoft_demo_wrong_unlock_order(void)
{
    /* BUG: Unlock order differs from lock order. */
    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    tx_mutex_get(&log_file_mutex, TX_WAIT_FOREVER);
    tx_mutex_put(&stats_mutex);
    tx_mutex_put(&log_file_mutex);
}

void parasoft_demo_destroy_locked_mutex(void)
{
    /* BUG: Destroying a locked mutex. */
    tx_mutex_get(&log_file_mutex, TX_WAIT_FOREVER);
    tx_mutex_delete(&log_file_mutex);
}

void parasoft_demo_stack_buffer_overflow(const char *input)
{
    char small_buffer[8];

    /* BUG: Unbounded copy into fixed-size stack buffer. */
    strcpy(small_buffer, input);
    printf("%s\n", small_buffer);
}

void parasoft_demo_reachable_bug_entry(int argc, char **argv)
{
    /* BUG DEMO: Direct calls make these defects visible to Parasoft. */
    (void)argc;
    parasoft_demo_race_condition();
    parasoft_demo_double_lock_deadlock();
    parasoft_demo_missing_unlock(1U);
    parasoft_demo_wrong_unlock_order();
    parasoft_demo_destroy_locked_mutex();
    parasoft_demo_stack_buffer_overflow(argv[0]);
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    char parasoft_demo_small_buffer[4];
    char *parasoft_demo_null_ptr = NULL;

    /* PARASOFT_DEMO_BUG: obvious stack buffer overflow. */
    strcpy(parasoft_demo_small_buffer, "this string is too long");

    /* PARASOFT_DEMO_BUG: obvious null pointer dereference. */
    *parasoft_demo_null_ptr = 'X';

    parasoft_demo_reachable_bug_entry(argc, argv);

    /* Disable stdout buffering for immediate output */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    printf("==> Starting ThreadX kernel...\n");
    fflush(stdout);
    
    /* Enter ThreadX kernel - never returns */
    tx_kernel_enter();
    
    return 0;
}
