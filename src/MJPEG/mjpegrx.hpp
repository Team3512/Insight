/* MJPEG HTTP stream decoder */

#ifndef _MJPEGRX_H
#define _MJPEGRX_H

#include "mjpeg_thread.h"
#include "mjpeg_mutex.h"

struct keyvalue_t {
    char *key;
    char *value;
    struct keyvalue_t *next;
};

struct mjpeg_callbacks_t {
    void (*donecallback)(void *optarg);
    void (*readcallback)(char *buf, int bufsize, void *optarg);
    void *optarg;
};

struct mjpeg_inst_t {
    char *host;
    char *reqpath;
    int port;

    struct mjpeg_callbacks_t callbacks;

    volatile int threadrunning;
    mjpeg_mutex_t mutex;
    mjpeg_thread_t thread;
    int cancelfdr;
    int cancelfdw;
    int sd;
};

struct mjpeg_inst_t *
mjpeg_launchthread(
        char *host,
        int port,
        char *reqpath,
        struct mjpeg_callbacks_t *callbacks
        );

int mjpeg_sck_recv(int sockfd, void *buf, size_t len, int cancelfd);
void mjpeg_stopthread(struct mjpeg_inst_t *inst);
void * mjpeg_threadmain(void *optarg);

#endif /* _MJPEGRX_H */
