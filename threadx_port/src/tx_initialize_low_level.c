/***************************************************************************
 * Copyright (c) 2024 Microsoft Corporation
 * Modified for MinGW GCC compatibility - SCD Training Demo
 *
 * SPDX-License-Identifier: MIT
 **************************************************************************/

/**************************************************************************/
/**                                                                       */
/** ThreadX Component                                                     */
/**                                                                       */
/**   Initialize - Win32/MinGW Port                                       */
/**                                                                       */
/**************************************************************************/

#define TX_SOURCE_CODE

#include "tx_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>

/* Define various Win32 objects used by the ThreadX port.  */
TX_WIN32_CRITICAL_SECTION       _tx_win32_critical_section;
HANDLE                          _tx_win32_scheduler_semaphore;
DWORD                           _tx_win32_scheduler_id;
ULONG                           _tx_win32_global_int_disabled_flag;
LARGE_INTEGER                   _tx_win32_time_stamp;
ULONG                           _tx_win32_system_error;
extern TX_THREAD                *_tx_thread_current_ptr;

/* Define simulated timer interrupt.  */
UINT                            _tx_win32_timer_id;
VOID CALLBACK                   _tx_win32_timer_interrupt(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

#ifdef TX_WIN32_DEBUG_ENABLE

extern ULONG                    _tx_thread_system_state;
extern UINT                     _tx_thread_preempt_disable;
extern TX_THREAD                *_tx_thread_current_ptr;
extern TX_THREAD                *_tx_thread_execute_ptr;

#ifndef TX_WIN32_DEBUG_EVENT_SIZE
#define TX_WIN32_DEBUG_EVENT_SIZE           400
#endif

typedef struct TX_WIN32_DEBUG_ENTRY_STRUCT
{
    char                                    *tx_win32_debug_entry_action;
    LARGE_INTEGER                           tx_win32_debug_entry_timestamp;
    char                                    *tx_win32_debug_entry_file;
    unsigned long                           tx_win32_debug_entry_line;
    TX_WIN32_CRITICAL_SECTION               tx_win32_debug_entry_critical_section;
    unsigned long                           tx_win32_debug_entry_int_disabled_flag;
    ULONG                                   tx_win32_debug_entry_system_state;
    UINT                                    tx_win32_debug_entry_preempt_disable;
    TX_THREAD                               *tx_win32_debug_entry_current_thread;
    DWORD                                   tx_win32_debug_entry_current_thread_id;
    TX_THREAD                               *tx_win32_debug_entry_execute_thread;
    DWORD                                   tx_win32_debug_entry_execute_thread_id;
    DWORD                                   tx_win32_debug_entry_running_id;
} TX_WIN32_DEBUG_ENTRY;

TX_WIN32_DEBUG_ENTRY    _tx_win32_debug_entry_array[TX_WIN32_DEBUG_EVENT_SIZE];
unsigned long           _tx_win32_debug_entry_index =  0;

void    _tx_win32_debug_entry_insert(char *action, char *file, unsigned long line)
{
    QueryPerformanceCounter((LARGE_INTEGER *)&_tx_win32_time_stamp);

    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_action =             action;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_timestamp =          _tx_win32_time_stamp;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_file =               file;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_line =               line;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_critical_section =   _tx_win32_critical_section;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_int_disabled_flag =  _tx_win32_global_int_disabled_flag;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_system_state =       _tx_thread_system_state;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_preempt_disable =    _tx_thread_preempt_disable;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_current_thread =     _tx_thread_current_ptr;
    if (_tx_thread_current_ptr)
        _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_current_thread_id =  _tx_thread_current_ptr -> tx_thread_win32_thread_id;
    else
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_current_thread_id =  0;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_execute_thread =     _tx_thread_execute_ptr;
    if (_tx_thread_execute_ptr)
        _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_execute_thread_id =  _tx_thread_execute_ptr -> tx_thread_win32_thread_id;
    else
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_execute_thread_id =  0;
    _tx_win32_debug_entry_array[_tx_win32_debug_entry_index].tx_win32_debug_entry_running_id =         GetCurrentThreadId();

    _tx_win32_debug_entry_index++;

    if (_tx_win32_debug_entry_index >= TX_WIN32_DEBUG_EVENT_SIZE)
    {
        _tx_win32_debug_entry_index =  0;
    }
}

#endif

/* Define the ThreadX timer interrupt handler.  */
void            _tx_timer_interrupt(void);

/* Define other external function references.  */
VOID            _tx_initialize_low_level(VOID);
VOID            _tx_thread_context_save(VOID);
VOID            _tx_thread_context_restore(VOID);

/* Define other external variable references.  */
extern VOID     *_tx_initialize_unused_memory;

/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _tx_initialize_low_level                          Win32/MinGW       */
/*                                                           6.4.2        */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function is responsible for any low-level processor            */
/*    initialization, including setting up interrupt vectors, setting     */
/*    up a periodic timer interrupt source, saving the system stack       */
/*    pointer for use in ISR processing later, and finding the first      */
/*    available RAM memory address for tx_application_define.             */
/*                                                                        */
/**************************************************************************/
VOID   _tx_initialize_low_level(VOID)
{

#ifndef TX_WIN32_BYPASS_AFFINITY_SETUP
    /* Limit this ThreadX simulation on Win32 to a single core.  */
    if (SetProcessAffinityMask( GetCurrentProcess(), 1 ) == 0)
    {
        printf("ThreadX Win32 error restricting the process to one core!\n");
        while(1)
        {
        }
    }
#endif

    /* Pickup the first available memory address.  */
    _tx_initialize_unused_memory =  malloc(TX_WIN32_MEMORY_SIZE);

    /* Pickup the unique Id of the current thread.  */
    _tx_win32_scheduler_id =        GetCurrentThreadId();

    /* Create the system critical section mutex.  */
    _tx_win32_critical_section.tx_win32_critical_section_mutex_handle =  CreateMutex(NULL, FALSE, NULL);
    _tx_win32_critical_section.tx_win32_critical_section_nested_count =  0;
    _tx_win32_critical_section.tx_win32_critical_section_owner =         0;

    /* Create the semaphore that regulates when the scheduler executes.  */
    _tx_win32_scheduler_semaphore =  CreateSemaphore(NULL, 0, 1, NULL);

    /* Initialize the global interrupt disabled flag.  */
    _tx_win32_global_int_disabled_flag =  TX_FALSE;
}

/* This routine is called after initialization is complete to start interrupts.  */
void _tx_initialize_start_interrupts(void)
{
    TIMECAPS tc;
    UINT     wTimerRes;

    /* Queries the timer device to determine its resolution.  */
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
    {
        printf("Query timer device error.");
        while (1)
        {
        }
    }

    wTimerRes = min(max(tc.wPeriodMin, TX_TIMER_PERIODIC), tc.wPeriodMax);

    /* Start a specified timer event.  */
    _tx_win32_timer_id = timeSetEvent(TX_TIMER_PERIODIC, wTimerRes, _tx_win32_timer_interrupt, 0, TIME_PERIODIC);
}

/* Define the ThreadX system timer interrupt.  */
VOID CALLBACK _tx_win32_timer_interrupt(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    (void)wTimerID;
    (void)msg;
    (void)dwUser;
    (void)dw1;
    (void)dw2;

    /* Call ThreadX context save for interrupt preparation.  */
    _tx_thread_context_save();

    /* Call the ThreadX system timer interrupt processing.  */
    _tx_timer_interrupt();

    /* Call ThreadX context restore for interrupt completion.  */
    _tx_thread_context_restore();
}
