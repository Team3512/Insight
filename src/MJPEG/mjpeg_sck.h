#ifndef _MJPEG_SCK_H
#define _MJPEG_SCK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32

#define _WIN32_LEAN_AND_MEAN
#include <winsock2.h>

typedef int socklen_t;
typedef u_int mjpeg_socket_t;

#include "win32_socketpair.h"

#else

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

typedef int mjpeg_socket_t;
#define INVALID_SOCKET -1

#endif

typedef enum mjpeg_sck_status {
    SCK_DONE,       // The socket has sent/received the data
    SCK_NOTREADY,   // The socket is not ready to send/receive data yet
    SCK_DISCONNECT, // The TCP socket has been disconnected
    SCK_ERROR       // An unexpected error occurred
} mjpeg_sck_status;

/* Returns 1 if socket is valid
 * Returns 0 if socket is invalid
 */
int mjpeg_sck_valid(mjpeg_socket_t sd);

/* enable != 0 sets socket to non-blocking
 * enable == 0 sets socket to blocking
 */
int mjpeg_sck_setnonblocking(mjpeg_socket_t sd, int enable);

/* Returns platform independent error condition */
mjpeg_sck_status mjpeg_sck_geterror();

int mjpeg_sck_close(mjpeg_socket_t sd);

#ifdef __cplusplus
}
#endif

#endif /* _MJPEG_SCK_H */
