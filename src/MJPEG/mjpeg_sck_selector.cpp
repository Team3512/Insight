// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#include "mjpeg_sck_selector.hpp"

mjpeg_sck_selector::mjpeg_sck_selector() {
    zero(select_type::read | select_type::write | select_type::except);
    m_maxSocket = 0;

    m_readfds = m_in_readfds;
    m_writefds = m_in_writefds;
    m_exceptfds = m_in_exceptfds;
}

void mjpeg_sck_selector::addSocket(mjpeg_socket_t sd, uint32_t types) {
    if (types & select_type::read) {
        FD_SET(sd, &m_in_readfds);
    }
    if (types & select_type::write) {
        FD_SET(sd, &m_in_writefds);
    }
    if (types & select_type::except) {
        FD_SET(sd, &m_in_exceptfds);
    }

    if (sd > m_maxSocket) {
        m_maxSocket = sd;
    }
}

void mjpeg_sck_selector::removeSocket(mjpeg_socket_t sd, uint32_t types) {
    if (types & select_type::read) {
        FD_CLR(sd, &m_in_readfds);
    }
    if (types & select_type::write) {
        FD_CLR(sd, &m_in_writefds);
    }
    if (types & select_type::except) {
        FD_CLR(sd, &m_in_exceptfds);
    }
}

bool mjpeg_sck_selector::isSelected(mjpeg_socket_t sd, select_type type) {
    if (type == select_type::read) {
        return FD_ISSET(sd, &m_in_readfds);
    } else if (type == select_type::write) {
        return FD_ISSET(sd, &m_in_writefds);
    } else if (type == select_type::except) {
        return FD_ISSET(sd, &m_in_exceptfds);
    } else {
        return false;
    }
}

bool mjpeg_sck_selector::isReady(mjpeg_socket_t sd, select_type type) {
    if (type == select_type::read) {
        return FD_ISSET(sd, &m_readfds);
    } else if (type == select_type::write) {
        return FD_ISSET(sd, &m_writefds);
    } else if (type == select_type::except) {
        return FD_ISSET(sd, &m_exceptfds);
    } else {
        return false;
    }
}

void mjpeg_sck_selector::zero(uint32_t types) {
    if (types & select_type::read) {
        FD_ZERO(&m_in_readfds);
    }
    if (types & select_type::write) {
        FD_ZERO(&m_in_writefds);
    }
    if (types & select_type::except) {
        FD_ZERO(&m_in_exceptfds);
    }
}

int mjpeg_sck_selector::select(struct timeval* timeout) {
    m_readfds = m_in_readfds;
    m_writefds = m_in_writefds;
    m_exceptfds = m_in_exceptfds;

    return ::select(m_maxSocket + 1, &m_readfds, &m_writefds, &m_exceptfds,
                    timeout);
}
