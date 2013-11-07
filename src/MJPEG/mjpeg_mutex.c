#include "mjpeg_mutex.h"

void mjpeg_mutex_init( mjpeg_mutex_t* mutex ) {
#ifdef _WIN32
    InitializeCriticalSection(&mutex->mutex);
#else
    pthread_mutex_init(&mutex->mutex , NULL);
#endif
}

void mjpeg_mutex_destroy( mjpeg_mutex_t* mutex ) {
#ifdef _WIN32
    DeleteCriticalSection(&mutex->mutex);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
}

void mjpeg_mutex_lock( mjpeg_mutex_t* mutex ) {
#ifdef _WIN32
    EnterCriticalSection(&mutex->mutex);
#else
    pthread_mutex_lock(&mutex->mutex);
#endif
}

void mjpeg_mutex_unlock( mjpeg_mutex_t* mutex ) {
#ifdef _WIN32
    LeaveCriticalSection(&mutex->mutex);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif
}
