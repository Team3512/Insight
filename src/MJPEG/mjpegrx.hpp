/* MJPEG HTTP stream decoder */

#ifndef _MJPEGRX_H
#define _MJPEGRX_H

#include <thread>
#include <atomic>

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
    char *m_host;
    char *m_reqpath;
    int m_port;

    struct mjpeg_callbacks_t m_callbacks;

    std::atomic<bool> m_threadrunning;
    std::thread* m_thread;
    int m_cancelfdr;
    int m_cancelfdw;
    int m_sd;

    mjpeg_inst_t(
            char *host,
            int port,
            char *reqpath,
            struct mjpeg_callbacks_t *callbacks
            );
    ~mjpeg_inst_t();

    void mjpeg_threadmain();
};

int mjpeg_sck_recv(int sockfd, void *buf, size_t len, int cancelfd);

#endif /* _MJPEGRX_H */
