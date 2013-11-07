#ifndef _MJPEG_MUTEX_H
#define _MJPEG_MUTEX_H

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <pthread.h>

#endif

typedef struct mjpeg_mutex_t_ {
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
} mjpeg_mutex_t;

void mjpeg_mutex_init( mjpeg_mutex_t* mutex );

void mjpeg_mutex_destroy( mjpeg_mutex_t* mutex );

void mjpeg_mutex_lock( mjpeg_mutex_t* mutex );

void mjpeg_mutex_unlock( mjpeg_mutex_t* mutex );

#endif /* _MJPEG_MUTEX_H */
