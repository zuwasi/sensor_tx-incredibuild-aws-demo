/***************************************************************************
 * Copyright (c) 2024 Microsoft Corporation
 * Modified for MinGW GCC compatibility - SCD Training Demo
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * SPDX-License-Identifier: MIT
 **************************************************************************/

/**************************************************************************/
/**                                                                       */
/** ThreadX Component                                                     */
/**                                                                       */
/**   Port Specific - Win32/MinGW GCC                                     */
/**                                                                       */
/**************************************************************************/

#ifndef TX_PORT_H
#define TX_PORT_H

/* Determine if the optional ThreadX user define file should be used.  */
#ifdef TX_INCLUDE_USER_DEFINE_FILE
#include "tx_user.h"
#endif

/* Define compiler library include files.  */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Define performance metric symbols.  */
#ifndef TX_BLOCK_POOL_ENABLE_PERFORMANCE_INFO
#define TX_BLOCK_POOL_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_BYTE_POOL_ENABLE_PERFORMANCE_INFO
#define TX_BYTE_POOL_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_EVENT_FLAGS_ENABLE_PERFORMANCE_INFO
#define TX_EVENT_FLAGS_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_MUTEX_ENABLE_PERFORMANCE_INFO
#define TX_MUTEX_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_QUEUE_ENABLE_PERFORMANCE_INFO
#define TX_QUEUE_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_SEMAPHORE_ENABLE_PERFORMANCE_INFO
#define TX_SEMAPHORE_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_THREAD_ENABLE_PERFORMANCE_INFO
#define TX_THREAD_ENABLE_PERFORMANCE_INFO
#endif

#ifndef TX_TIMER_ENABLE_PERFORMANCE_INFO
#define TX_TIMER_ENABLE_PERFORMANCE_INFO
#endif

/* Enable trace info.  */
#ifndef TX_ENABLE_EVENT_TRACE
#define TX_ENABLE_EVENT_TRACE
#endif

/* Define ThreadX basic types for this port.  */
#define VOID                                    void
typedef char                                    CHAR;
typedef unsigned char                           UCHAR;
typedef int                                     INT;
typedef unsigned int                            UINT;
typedef long                                    LONG;
typedef unsigned long                           ULONG;
typedef short                                   SHORT;
typedef unsigned short                          USHORT;

/* Add Win32 debug insert prototype.  */
void    _tx_win32_debug_entry_insert(char *action, char *file, unsigned long line);

#ifndef TX_WIN32_DEBUG_ENABLE
/* If Win32 debug is not enabled, turn logging into white-space.  */
#define _tx_win32_debug_entry_insert(a, b, c)
#endif

/* Define the TX_MEMSET macro to remove library reference.  */
#define TX_MEMSET(a,b,c)                        {                                       \
                                                UCHAR *ptr;                             \
                                                UCHAR value;                            \
                                                UINT  i, size;                          \
                                                    ptr =    (UCHAR *) ((VOID *) a);    \
                                                    value =  (UCHAR) b;                 \
                                                    size =   (UINT) c;                  \
                                                    for (i = 0; i < size; i++)          \
                                                    {                                   \
                                                        *ptr++ =  value;                \
                                                    }                                   \
                                                }

/* Include windows include file.  */
#include <windows.h>

/* Define the priority levels for ThreadX.  Legal values range
   from 32 to 1024 and MUST be evenly divisible by 32.  */
#ifndef TX_MAX_PRIORITIES
#define TX_MAX_PRIORITIES                       32
#endif

/* Define the minimum stack for a ThreadX thread on this processor.  */
#ifndef TX_MINIMUM_STACK
#define TX_MINIMUM_STACK                        200
#endif

/* Define the system timer thread's default stack size and priority.  */
#ifndef TX_TIMER_THREAD_STACK_SIZE
#define TX_TIMER_THREAD_STACK_SIZE              400
#endif

#ifndef TX_TIMER_THREAD_PRIORITY
#define TX_TIMER_THREAD_PRIORITY                0
#endif

/* Define various constants for the ThreadX port.  */
#define TX_INT_DISABLE                          1
#define TX_INT_ENABLE                           0

/* Define the clock source for trace event entry time stamp.  */
#ifndef TX_TRACE_TIME_SOURCE
#define TX_TRACE_TIME_SOURCE                    ((ULONG) (_tx_win32_time_stamp.LowPart));
#endif

#ifndef TX_TRACE_TIME_MASK
#define TX_TRACE_TIME_MASK                      0xFFFFFFFFUL
#endif

/* Define the port-specific trace extension.  */
#define TX_TRACE_PORT_EXTENSION                 QueryPerformanceCounter((LARGE_INTEGER *)&_tx_win32_time_stamp);

/* Define the port specific options for the _tx_build_options variable.  */
#define TX_PORT_SPECIFIC_BUILD_OPTIONS          0

/* Define the in-line initialization constant.  */
#define TX_INLINE_INITIALIZATION

/* Define the Win32-specific initialization code.  */
void    _tx_initialize_start_interrupts(void);
#define TX_PORT_SPECIFIC_PRE_SCHEDULER_INITIALIZATION   _tx_initialize_start_interrupts();

/* Determine whether or not stack checking is enabled.  */
#ifdef TX_ENABLE_STACK_CHECKING
#undef TX_DISABLE_STACK_FILLING
#endif

/* Define the TX_THREAD control block extensions for this port.  */
#define TX_THREAD_EXTENSION_0                   HANDLE tx_thread_win32_thread_handle; \
                                                DWORD  tx_thread_win32_thread_id; \
                                                HANDLE tx_thread_win32_thread_run_semaphore; \
                                                UINT   tx_thread_win32_suspension_type; \
                                                UINT   tx_thread_win32_int_disabled_flag;

#define TX_THREAD_EXTENSION_1
#define TX_THREAD_EXTENSION_2
#define TX_THREAD_EXTENSION_3

/* Define the port extensions of the remaining ThreadX objects.  */
#define TX_BLOCK_POOL_EXTENSION
#define TX_BYTE_POOL_EXTENSION
#define TX_EVENT_FLAGS_GROUP_EXTENSION
#define TX_MUTEX_EXTENSION
#define TX_QUEUE_EXTENSION
#define TX_SEMAPHORE_EXTENSION
#define TX_TIMER_EXTENSION

/* Define the user extension field of the thread control block.  */
#ifndef TX_THREAD_USER_EXTENSION
#define TX_THREAD_USER_EXTENSION
#endif

/* Define the macros for processing extensions in thread functions.  */
#define TX_THREAD_CREATE_EXTENSION(thread_ptr)
#define TX_THREAD_DELETE_EXTENSION(thread_ptr)
#define TX_THREAD_COMPLETED_EXTENSION(thread_ptr)
#define TX_THREAD_TERMINATED_EXTENSION(thread_ptr)

/* Define the ThreadX object creation extensions for the remaining objects.  */
#define TX_BLOCK_POOL_CREATE_EXTENSION(pool_ptr)
#define TX_BYTE_POOL_CREATE_EXTENSION(pool_ptr)
#define TX_EVENT_FLAGS_GROUP_CREATE_EXTENSION(group_ptr)
#define TX_MUTEX_CREATE_EXTENSION(mutex_ptr)
#define TX_QUEUE_CREATE_EXTENSION(queue_ptr)
#define TX_SEMAPHORE_CREATE_EXTENSION(semaphore_ptr)
#define TX_TIMER_CREATE_EXTENSION(timer_ptr)

/* Define the ThreadX object deletion extensions for the remaining objects.  */
#define TX_BLOCK_POOL_DELETE_EXTENSION(pool_ptr)
#define TX_BYTE_POOL_DELETE_EXTENSION(pool_ptr)
#define TX_EVENT_FLAGS_GROUP_DELETE_EXTENSION(group_ptr)
#define TX_MUTEX_DELETE_EXTENSION(mutex_ptr)
#define TX_QUEUE_DELETE_EXTENSION(queue_ptr)
#define TX_SEMAPHORE_DELETE_EXTENSION(semaphore_ptr)
#define TX_TIMER_DELETE_EXTENSION(timer_ptr)

struct TX_THREAD_STRUCT;

/* Define the Win32 critical section data structure.  */
typedef struct TX_WIN32_CRITICAL_SECTION_STRUCT
{
    HANDLE                                      tx_win32_critical_section_mutex_handle;
    DWORD                                       tx_win32_critical_section_owner;
    ULONG                                       tx_win32_critical_section_nested_count;
} TX_WIN32_CRITICAL_SECTION;

/* Define Win32-specific critical section APIs.  */
void  _tx_win32_critical_section_obtain(TX_WIN32_CRITICAL_SECTION *critical_section);
void  _tx_win32_critical_section_release(TX_WIN32_CRITICAL_SECTION *critical_section);
void  _tx_win32_critical_section_release_all(TX_WIN32_CRITICAL_SECTION *critical_section);

/* Define post completion processing for tx_thread_delete.  */
#define TX_THREAD_DELETE_PORT_COMPLETION(thread_ptr)                            \
{                                                                               \
BOOL            win32_status;                                                   \
DWORD           exitcode;                                                       \
HANDLE          threadrunsemaphore;                                             \
HANDLE          threadhandle;                                                   \
    threadhandle =       thread_ptr -> tx_thread_win32_thread_handle;           \
    threadrunsemaphore = thread_ptr -> tx_thread_win32_thread_run_semaphore;    \
    _tx_thread_interrupt_restore(tx_saved_posture);                             \
    do                                                                          \
    {                                                                           \
        win32_status =  GetExitCodeThread(threadhandle, &exitcode);             \
        if ((win32_status) && (exitcode != STILL_ACTIVE))                       \
        {                                                                       \
            break;                                                              \
        }                                                                       \
        ResumeThread(threadhandle);                                             \
        ReleaseSemaphore(threadrunsemaphore, 1, NULL);                          \
        Sleep(1);                                                               \
    } while (1);                                                                \
    CloseHandle(threadhandle);                                                  \
    tx_saved_posture =   _tx_thread_interrupt_disable();                        \
}

/* Define post completion processing for tx_thread_reset.  */
#define TX_THREAD_RESET_PORT_COMPLETION(thread_ptr)                             \
{                                                                               \
BOOL            win32_status;                                                   \
DWORD           exitcode;                                                       \
HANDLE          threadrunsemaphore;                                             \
HANDLE          threadhandle;                                                   \
    threadhandle =       thread_ptr -> tx_thread_win32_thread_handle;           \
    threadrunsemaphore = thread_ptr -> tx_thread_win32_thread_run_semaphore;    \
    _tx_thread_interrupt_restore(tx_saved_posture);                             \
    do                                                                          \
    {                                                                           \
        win32_status =  GetExitCodeThread(threadhandle, &exitcode);             \
        if ((win32_status) && (exitcode != STILL_ACTIVE))                       \
        {                                                                       \
            break;                                                              \
        }                                                                       \
        ResumeThread(threadhandle);                                             \
        ReleaseSemaphore(threadrunsemaphore, 1, NULL);                          \
        Sleep(1);                                                               \
    } while (1);                                                                \
    CloseHandle(threadhandle);                                                  \
    tx_saved_posture =   _tx_thread_interrupt_disable();                        \
}

/* Define ThreadX interrupt lockout and restore macros.  */
UINT   _tx_thread_interrupt_disable(void);
VOID   _tx_thread_interrupt_restore(UINT previous_posture);

#define TX_INTERRUPT_SAVE_AREA UINT             tx_saved_posture;
#define TX_DISABLE                              tx_saved_posture =   _tx_thread_interrupt_disable();
#define TX_RESTORE                              _tx_thread_interrupt_restore(tx_saved_posture);

/* Define the interrupt lockout macros for each ThreadX object.  */
#define TX_BLOCK_POOL_DISABLE                   TX_DISABLE
#define TX_BYTE_POOL_DISABLE                    TX_DISABLE
#define TX_EVENT_FLAGS_GROUP_DISABLE            TX_DISABLE
#define TX_MUTEX_DISABLE                        TX_DISABLE
#define TX_QUEUE_DISABLE                        TX_DISABLE
#define TX_SEMAPHORE_DISABLE                    TX_DISABLE

/* Define the version ID of ThreadX.  */
#ifdef TX_THREAD_INIT
CHAR                            _tx_version_id[] =
                                    "Copyright (c) 2024 Microsoft Corporation.  *  ThreadX Win32/MinGW Version 6.4.2 *";
#else
extern  CHAR                    _tx_version_id[];
#endif

/* Define externals for the Win32 port of ThreadX.  */
extern TX_WIN32_CRITICAL_SECTION                _tx_win32_critical_section;
extern HANDLE                                   _tx_win32_scheduler_semaphore;
extern DWORD                                    _tx_win32_scheduler_id;
extern ULONG                                    _tx_win32_global_int_disabled_flag;
extern LARGE_INTEGER                            _tx_win32_time_stamp;
extern ULONG                                    _tx_win32_system_error;
extern HANDLE                                   _tx_win32_timer_handle;
extern UINT                                     _tx_win32_timer_id;

#ifndef TX_WIN32_MEMORY_SIZE
#define TX_WIN32_MEMORY_SIZE                    64000
#endif

#ifndef TX_TIMER_PERIODIC
#define TX_TIMER_PERIODIC                       10
#endif

#endif /* TX_PORT_H */
