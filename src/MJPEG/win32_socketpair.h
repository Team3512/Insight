// Copyright (c) FRC Team 3512, Spartatroniks 2016. All Rights Reserved.

#ifndef _WIN32SOCKETPAIR_H
#define _WIN32SOCKETPAIR_H

#ifdef __cplusplus
extern "C" {
#endif

int dumb_socketpair(SOCKET socks[2], int make_overlapped);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32SOCKET_PAIR_H */
