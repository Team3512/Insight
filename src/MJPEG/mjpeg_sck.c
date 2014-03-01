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

mjpeg_sck_status
mjpeg_sck_geterror()
{
#ifdef _WIN32
    switch (WSAGetLastError()) {
        case WSAEWOULDBLOCK :  return SCK_NOTREADY;
        case WSAECONNABORTED : return SCK_DISCONNECT;
        case WSAECONNRESET :   return SCK_DISCONNECT;
        case WSAETIMEDOUT :    return SCK_DISCONNECT;
        case WSAENETRESET :    return SCK_DISCONNECT;
        case WSAENOTCONN :     return SCK_DISCONNECT;
        default :              return SCK_ERROR;
    }
#else
    // The followings are sometimes equal to EWOULDBLOCK,
    // so we have to make a special case for them in order
    // to avoid having double values in the switch case
    if ((errno == EAGAIN) || (errno == EINPROGRESS)) {
        return SCK_NOTREADY;
    }

    switch (errno) {
        case EWOULDBLOCK :  return SCK_NOTREADY;
        case ECONNABORTED : return SCK_DISCONNECT;
        case ECONNRESET :   return SCK_DISCONNECT;
        case ETIMEDOUT :    return SCK_DISCONNECT;
        case ENETRESET :    return SCK_DISCONNECT;
        case ENOTCONN :     return SCK_DISCONNECT;
        case EPIPE :        return SCK_DISCONNECT;
        default :           return SCK_ERROR;
    }
#endif
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
