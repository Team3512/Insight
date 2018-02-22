// Copyright (c) 2013-2018 FRC Team 3512. All Rights Reserved.

#pragma once

#include <stdint.h>

#include "mjpeg_sck.hpp"

/**
 * A wrapper around select(3)
 */
class mjpeg_sck_selector {
public:
    mjpeg_sck_selector();

    enum select_type { read = 1 << 0, write = 1 << 1, except = 1 << 2 };

    void addSocket(mjpeg_socket_t sd, uint32_t types);
    void removeSocket(mjpeg_socket_t sd, uint32_t types);

    // Returns true if socket is going to be checked
    bool isSelected(mjpeg_socket_t sd, select_type type);

    // Returns true if socket is pending an action
    bool isReady(mjpeg_socket_t sd, select_type type);

    void zero(uint32_t types);

    /* Returns the number of sockets ready; timeout of 0 causes blocking on
     * select(3)
     */
    int select(struct timeval* timeout);

private:
    fd_set m_readfds;    // set containing sockets to be checked for being ready
                         // to read
    fd_set m_writefds;   // set containing sockets to be checked for being ready
                         // to write
    fd_set m_exceptfds;  // set containing sockets to be checked for error
                         // conditions pending
    mjpeg_socket_t m_maxSocket;  // maximum socket handle

    fd_set m_in_readfds;    // set containing sockets to be checked for being
                            // ready to read
    fd_set m_in_writefds;   // set containing sockets to be checked for being
                            // ready to write
    fd_set m_in_exceptfds;  // set containing sockets to be checked for error
                            // conditions pending
};
