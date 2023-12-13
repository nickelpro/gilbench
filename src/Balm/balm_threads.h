#ifndef BALM_THREADS_H
#define BALM_THREADS_H


#if __has_include(<threads.h>)
#include <threads.h>
#elif __has_include(<pthread.h>)
#include <pthread.h>
typedef pthread_mutex_t mtx_t;
#define mtx_plain NULL
#define mtx_init(mtx_ptr, attrs) pthread_mutex_init(mtx_ptr, attrs)
#define mtx_lock(mtx_ptr) pthread_mutex_lock(mtx_ptr)
#define mtx_unlock(mtx_ptr) pthread_mutex_unlock(mtx_ptr)
#elif __has_include(<windows.h>)
#include <windows.h>
typedef CRITICAL_SECTION mtx_t;
#define mtx_plain NULL
#define mtx_init(mtx_ptr, attrs)                                               \
  InitializeCriticalSectionAndSpinCount(mtx_ptr, 0x400)
#define mtx_lock(mtx_ptr) EnterCriticalSection(mtx_ptr)
#define mtx_unlock(mtx_ptr) LeaveCriticalSection(mtx_ptr)
#else
#error "Here's a nickel kid. Go buy yourself a real computer."
#endif // __has_include

#endif // BALM_THREADS_H
