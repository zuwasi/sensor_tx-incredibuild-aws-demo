/*
 * Analysis stub for <windows.h>.
 *
 * The Parasoft C/C++test static-analysis frontend cannot preprocess the
 * full MinGW-w64 <windows.h>, which makes Flow Analysis silently produce
 * zero results (0 graphs) for every ThreadX file, because the ThreadX
 * Win32 port includes <windows.h> from tx_port.h.
 *
 * This stub declares only the Win32 types and APIs the ThreadX Win32
 * port actually uses, so analysis tools can parse the code. It is only
 * prepended to the include path during analysis builds (-Ianalysis_stubs
 * comes first); the real application build keeps using the system
 * <windows.h>.
 */
#ifndef STUB_WINDOWS_H
#define STUB_WINDOWS_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define CALLBACK
#define WINAPI
#define INFINITE                 0xFFFFFFFFu
#define TIME_PERIODIC            0x0001u
#define TIMERR_NOERROR           0
#define WAIT_OBJECT_0            0u
#define STATUS_WAIT_0            0u
#define THREAD_PRIORITY_NORMAL   0
#define THREAD_PRIORITY_LOWEST   -2
#define THREAD_PRIORITY_HIGHEST  2
#define CREATE_SUSPENDED         0x00000004u
/* winperf.h leaks this constant through <windows.h>; sensor_threadx.c uses
   it as an event-flag bit (value must stay 8 to preserve runtime behavior). */
#define PERF_TIMER_TICK          0x00000008u
#define MAXIMUM_WAIT_OBJECTS     64

typedef void                VOID_S_;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef void*               PVOID;
typedef unsigned long       DWORD;
typedef unsigned long*      LPDWORD;
typedef unsigned long       DWORD_PTR;
typedef unsigned int        UINT;
typedef int                 BOOL;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef unsigned long       ULONG_PTR;
typedef long long           LONGLONG;
typedef unsigned long long  ULONGLONG;

typedef void* HANDLE;

typedef union _LARGE_INTEGER {
    unsigned long LowPart;
    long          HighPart;
    LONGLONG      QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct timecaps_tag {
    UINT wPeriodMin;
    UINT wPeriodMax;
} TIMECAPS, *PTIMECAPS, *NPTIMECAPS, *LPTIMECAPS;

typedef UINT MMRESULT;

/* winmm.dll multimedia timer (linked with -lwinmm) */
MMRESULT timeSetEvent(UINT uDelay, UINT uResolution,
                      void (*fptCallback)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR),
                      DWORD_PTR dwUser, UINT fuEvent);
MMRESULT timeKillEvent(UINT uTimerID);
MMRESULT timeGetDevCaps(PTIMECAPS ptc, UINT cbtc);
MMRESULT timeBeginPeriod(UINT uPeriod);
MMRESULT timeEndPeriod(UINT uPeriod);

/* kernel32.dll threads and synchronization */
HANDLE CreateMutexA(void* lpMutexAttributes, BOOL bInitialOwner, const char* lpName);
HANDLE CreateMutexW(void* lpMutexAttributes, BOOL bInitialOwner, const unsigned short* lpName);
#define CreateMutex  CreateMutexA
HANDLE CreateSemaphoreA(void* lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, const char* lpName);
HANDLE CreateSemaphoreW(void* lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, const unsigned short* lpName);
#define CreateSemaphore CreateSemaphoreA
HANDLE CreateThread(void* lpThreadAttributes, unsigned long dwStackSize,
                    DWORD (*lpStartAddress)(LPVOID lpThreadParameter),
                    LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);
void   ExitThread(DWORD dwExitCode);
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
DWORD  GetCurrentThreadId(void);
int    GetThreadPriority(HANDLE hThread);
BOOL   SetThreadPriority(HANDLE hThread, int nPriority);
BOOL   ResumeThread(HANDLE hThread);
BOOL   SuspendThread(HANDLE hThread);
void   Sleep(DWORD dwMilliseconds);
DWORD  WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
BOOL   ReleaseMutex(HANDLE hMutex);
BOOL   ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount);
BOOL   QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
BOOL   QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
DWORD  GetTickCount(void);
BOOL   GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
BOOL   CloseHandle(HANDLE hObject);
#define STILL_ACTIVE 0x00000103u
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#endif /* STUB_WINDOWS_H */
