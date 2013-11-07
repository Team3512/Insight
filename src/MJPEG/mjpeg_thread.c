
#include "mjpeg_thread.h"

int
mjpeg_thread_create(mjpeg_thread_t *thread,
  void *(*start_routine) (void *),
  void *arg)
{
#ifdef __WIN32
  thread->optarg = arg;
  thread->start_routine = start_routine;
  thread->thread = (HANDLE)(_beginthreadex(NULL, 0, win32_threadfunc, (void *) thread, 0, &thread->threadId));
  if((int)thread->thread == -1) return -1;
  return 0;
#else
  return pthread_create(&thread->thread, NULL, start_routine, arg);
#endif
}

int
mjpeg_thread_join(mjpeg_thread_t *thread, void **retval)
{
#ifdef _WIN32
  int error;

  /* Make sure we didn't already join the thread. */
  if(!thread->thread) return -1;

  /* Block on the thread. */
  error = WaitForSingleObject(thread->thread, INFINITE);
  if(error == WAIT_FAILED) {
    return -1;
  }

  /* Clean up the thread handle. */
  error = CloseHandle(thread->thread);
  if(error == 0) {
    return -1;
  }

  /* Pass back the (void *) the function returned. */
  if(retval != NULL)
    *retval = thread->retval;

  /* Mark the thread as defunct. */
  thread->thread = 0;
  return 0;
#else
  return pthread_join(thread->thread, retval);
#endif
}

#ifdef _WIN32
unsigned int
__stdcall win32_threadfunc(void *optarg)
{
  mjpeg_thread_t *thread = (mjpeg_thread_t *) optarg;
  thread->retval = thread->start_routine(thread->optarg);
  return 0;
}
#endif

