#ifndef _MJPEG_THREAD_H
#define _MJPEG_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#ifdef WIN32
#define mjpeg_thread_exit(a) return a
#else
#define mjpeg_thread_exit(a) pthread_exit(a)
#endif

typedef struct mjpeg_thread_t_ {
#ifdef WIN32
  HANDLE thread;
  void *optarg;
  void *retval;
  void *(*start_routine) (void *);
  unsigned int threadId;
#else
  pthread_t thread;
#endif
} mjpeg_thread_t;

int
mjpeg_thread_create(mjpeg_thread_t *thread,
  void *(*start_routine) (void *),
  void *arg);

int
mjpeg_thread_join(mjpeg_thread_t *thread,
  void **retval);

#ifdef WIN32
unsigned int
__stdcall win32_threadfunc(void *optarg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _MJPEG_THREAD_H */
