#include "mjpeg_sleep.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <time.h>
#include <errno.h>

#endif

void mjpeg_sleep(unsigned int milliseconds) {
#ifdef WIN32
    Sleep(milliseconds);
#else
    unsigned long long usecs = milliseconds / 1000;

    // Construct the time to wait
    struct timespec ti;
    ti.tv_nsec = (usecs % 1000000) * 1000;
    ti.tv_sec = usecs / 1000000;

    /* Wait...
     * If nanosleep returns -1, we check errno. If it is EINTR, nanosleep was
     * interrupted and has set ti to the remaining duration. We continue
     * sleeping until the complete duration has passed. We stop sleeping if it
     * was due to a different error.
     */
    while ( nanosleep( &ti , &ti ) == -1 && errno == EINTR ) {}
#endif
}
