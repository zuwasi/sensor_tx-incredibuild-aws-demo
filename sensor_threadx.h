/*
 * sensor_threadx.h - ThreadX RTOS Sensor Application Header
 * SCD Training Demo - Eclipse ThreadX RTOS Integration
 * 02-Feb-2026
 *
 * ThreadX Features Demonstrated:
 * - Multiple threads with different priorities
 * - Counting semaphores for synchronization
 * - Mutexes for mutual exclusion (priority inheritance)
 * - Message queues for inter-thread communication
 * - Byte pools for dynamic memory allocation
 * - Application timers for periodic operations
 * - Event flags for thread signaling
 * - Interrupt simulation
 */

#ifndef SENSOR_THREADX_H
#define SENSOR_THREADX_H

#include "tx_api.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Configuration Constants
 *===========================================================================*/

/* Thread stack sizes */
#define SENSOR_THREAD_STACK_SIZE        4096
#define MONITOR_THREAD_STACK_SIZE       4096
#define DISPLAY_THREAD_STACK_SIZE       4096
#define LOGGER_THREAD_STACK_SIZE        4096
#define TIMER_ISR_THREAD_STACK_SIZE     2048
#define KEYBOARD_THREAD_STACK_SIZE      2048

/* Thread priorities (lower number = higher priority) */
#define SENSOR_ISR_PRIORITY             1   /* Highest - simulated interrupt */
#define SENSOR_READ_PRIORITY            4   /* High priority for sensor reading */
#define DATA_PROCESS_PRIORITY           8   /* Medium priority for processing */
#define DISPLAY_PRIORITY                12  /* Lower priority for display */
#define LOGGER_PRIORITY                 16  /* Lowest priority for logging */
#define KEYBOARD_MONITOR_PRIORITY       2   /* High - just below ISR */
#define STATS_PRIORITY                  20  /* Background statistics */

/* Message queue configuration */
#define SENSOR_QUEUE_DEPTH              32
#define LOG_QUEUE_DEPTH                 64

/* Byte pool size */
#define BYTE_POOL_SIZE                  32768

/* Block pool configuration */
#define BLOCK_SIZE                      64
#define BLOCK_COUNT                     32

/* Semaphore counts */
#define SENSOR_DATA_SEM_INITIAL         0   /* Counting semaphore for data ready */
#define MAX_SENSOR_BUFFERS              5   /* Max buffers for counting semaphore */

/* Timer configuration */
#define SENSOR_SAMPLE_PERIOD            10  /* Timer ticks between samples */
#define STATS_REPORT_PERIOD             100 /* Timer ticks between stats reports */
#define WATCHDOG_TIMEOUT                500 /* Watchdog timer timeout */

/* Event flags */
#define SENSOR_EVENT_SENSOR_DATA_READY         0x0001
#define SENSOR_EVENT_PROCESSING_COMPLETE       0x0002
#define SENSOR_EVENT_DISPLAY_UPDATE            0x0004
#define APP_EVENT_LOG_FLUSH             0x0008 /* Renamed to avoid reserved macro name */
#define SENSOR_EVENT_SYSTEM_SHUTDOWN           0x0010
#define SENSOR_EVENT_ERROR_OCCURRED            0x0020
#define SENSOR_EVENT_TIMER_TICK                0x0040
#define SENSOR_EVENT_WATCHDOG_KICK             0x0080
#define SENSOR_EVENT_SELF_CHECK                0x0100

/* Sensor status codes */
#define SENSOR_STATUS_OK                0
#define SENSOR_STATUS_ERROR             1
#define SENSOR_STATUS_STOPPED           2
#define SENSOR_STATUS_TIMEOUT           3
#define SENSOR_STATUS_SELF_CHECK        4

/* Classification thresholds */
#define THRESHOLD_LOW_HIGH              14
#define MAX_SENSOR_VALUE                30

/* Value classification */
#define VALUE_LOW                       0
#define VALUE_HIGH                      1
#define VALUE_ERROR                     2

/*=============================================================================
 * Data Structures
 *===========================================================================*/

/* Sensor data message structure (for message queue) */
typedef struct SensorMessage_s {
    ULONG   timestamp;          /* System tick when reading was taken */
    INT     sensor_value;       /* Raw sensor reading */
    INT     classification;     /* 0=LOW, 1=HIGH, 2=ERROR */
    UINT    sequence_number;    /* Reading sequence number */
} SensorMessage;

/* Timing statistics structure (protected by mutex) */
typedef struct TimingStats_s {
    double  current_us;
    double  min_us;
    double  max_us;
    double  avg_us;
    double  total_us;
    ULONG   count;
    double  last_avg_us;
} ThreadXTimingStats;

/* System statistics (protected by mutex) */
typedef struct SystemStats_s {
    ULONG   total_readings;
    ULONG   low_count;
    ULONG   high_count;
    ULONG   error_count;
    ULONG   queue_overflows;
    ULONG   semaphore_timeouts;
    ULONG   mutex_contentions;
    ULONG   timer_expirations;
} SystemStats;

/* Log entry structure (for log message queue) */
typedef struct LogEntry_s {
    ULONG   timestamp;
    UINT    severity;           /* 0=INFO, 1=WARNING, 2=ERROR */
    CHAR    message[48];        /* Log message text */
} LogEntry;

/*=============================================================================
 * ThreadX Object Declarations (extern)
 *===========================================================================*/

/* Threads */
extern TX_THREAD        sensor_isr_thread;      /* Simulated sensor interrupt */
extern TX_THREAD        sensor_read_thread;     /* Reads sensor data */
extern TX_THREAD        data_process_thread;    /* Processes sensor data */
extern TX_THREAD        display_thread;         /* Displays results */
extern TX_THREAD        logger_thread;          /* Logs data to file */
extern TX_THREAD        stats_thread;           /* Computes statistics */
extern TX_THREAD        keyboard_monitor_thread; /* Keyboard quit monitor */

/* Message Queues */
extern TX_QUEUE         sensor_data_queue;      /* Sensor readings queue */
extern TX_QUEUE         log_queue;              /* Log messages queue */

/* Semaphores */
extern TX_SEMAPHORE     sensor_data_semaphore;  /* Counting: data buffers ready */
extern TX_SEMAPHORE     processing_semaphore;   /* Binary: processing slot */

/* Mutexes */
extern TX_MUTEX         stats_mutex;            /* Protects timing statistics */
extern TX_MUTEX         console_mutex;          /* Protects console output */
extern TX_MUTEX         log_file_mutex;         /* Protects log file access */

/* Event Flags */
extern TX_EVENT_FLAGS_GROUP system_events;      /* System-wide events */

/* Timers */
extern TX_TIMER         sensor_sample_timer;    /* Periodic sensor sampling */
extern TX_TIMER         stats_report_timer;     /* Periodic stats reporting */
extern TX_TIMER         watchdog_timer;         /* Watchdog timer */

/* Memory Pools */
extern TX_BYTE_POOL     system_byte_pool;       /* Dynamic memory allocation */
extern TX_BLOCK_POOL    sensor_block_pool;      /* Fixed-size sensor buffers */

/* Statistics */
extern ThreadXTimingStats   timing_stats;
extern SystemStats          system_stats;

/*=============================================================================
 * Function Prototypes
 *===========================================================================*/

/* Thread entry points */
void    sensor_isr_thread_entry(ULONG thread_input);
void    sensor_read_thread_entry(ULONG thread_input);
void    data_process_thread_entry(ULONG thread_input);
void    display_thread_entry(ULONG thread_input);
void    logger_thread_entry(ULONG thread_input);
void    stats_thread_entry(ULONG thread_input);
void    keyboard_monitor_thread_entry(ULONG thread_input);

/* Self-check function (intentional buffer overrun for demo) */
void    perform_self_check(void);

/* Timer callback functions */
void    sensor_sample_timer_callback(ULONG timer_input);
void    stats_report_timer_callback(ULONG timer_input);
void    watchdog_timer_callback(ULONG timer_input);

/* Utility functions */
INT     read_sensor(INT *value);
INT     classify_value(INT value);
void    print_message(INT classification, INT value);
void    log_message(UINT severity, const char *message);

/* Statistics functions */
void    timing_stats_init(void);
void    timing_stats_record(double elapsed_us);
void    timing_stats_print_summary(void);
void    print_threadx_services_summary(void);

/* Initialization */
void    sensor_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_THREADX_H */
