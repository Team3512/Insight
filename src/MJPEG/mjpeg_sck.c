#include "mjpeg_sck.h"

int
mjpeg_sck_valid(mjpeg_socket_t sd)
{
#ifdef _WIN32
    if (sd == INVALID_SOCKET) {
        return 0;
    }
#else
    if (sd < 0) {
        return 0;
    }
#endif
    return 1;
}

int
mjpeg_sck_setnonblocking(mjpeg_socket_t sd, int enable)
{
    int error;
#ifdef _WIN32
    u_long nbenable = enable;
    error = ioctlsocket(sd, FIONBIO, &nbenable);
    if (error != 0) {
        mjpeg_sck_close(sd);
        return -1;
    }
#else
    int flags = fcntl(sd, F_GETFL, 0);
    if (flags == -1) {
        mjpeg_sck_close(sd);
        return -1;
    }
    if (enable != 0) {
        error = fcntl(sd, F_SETFL, flags | O_NONBLOCK);
    }
    else {
        error = fcntl(sd, F_SETFL, flags & ~O_NONBLOCK);
    }
    if(error == -1) {
        mjpeg_sck_close(sd);
        return -1;
    }
#endif
    return 0;
}

int
mjpeg_sck_close(mjpeg_socket_t sd)
{
#ifdef _WIN32
    return closesocket(sd);
#else
    return close(sd);
#endif
}
