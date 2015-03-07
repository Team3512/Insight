#ifndef MJPEG_SCK_HPP
#define MJPEG_SCK_HPP

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
#include <arpa/inet.h>
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

/* mjpeg_sck_connect() attempts to connect to the specified
 *  remote host on the specified port. The function blocks
 *  until either cancelfd becomes ready for reading, or the
 *  connection succeeds or times out.
 *  If the connection succeeds, the new socket descriptor
 *  is returned. On error, -1 is returned, and errno is
 *  set appropriately. */
mjpeg_socket_t mjpeg_sck_connect(const char* host,
                                 int port,
                                 mjpeg_socket_t cancelfd);

int mjpeg_sck_close(mjpeg_socket_t sd);

/* A platform independent wrapper function which acts like
 *  the call socketpair(AF_INET, SOCK_STREAM, 0, sv) . */
mjpeg_socket_t mjpeg_pipe(mjpeg_socket_t sv[2]);

#endif // MJPEG_SCK_HPP

