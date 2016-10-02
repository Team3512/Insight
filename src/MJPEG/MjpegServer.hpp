// Copyright (c) FRC Team 3512, Spartatroniks 2013-2016. All Rights Reserved.

#pragma once

#include <stdint.h>

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <thread>

#include <jpeglib.h>

#include "mjpeg_sck_selector.hpp"

/**
 * An MJPEG server implementation
 */
class MjpegServer {
public:
    explicit MjpegServer(uint16_t port);
    virtual ~MjpegServer();

    void start();
    void stop();

    // Converts BGR image to JPEG before serving it
    void serveImage(uint8_t* image, unsigned int width, unsigned int height);

private:
    mjpeg_sck_selector m_clientSelector;
    std::list<mjpeg_socket_t> m_clientSockets;
    std::mutex m_clientSocketMutex;

    mjpeg_socket_t m_listenSock = INVALID_SOCKET;
    uint16_t m_port;

    mjpeg_socket_t m_cancelfdr = 0;
    mjpeg_socket_t m_cancelfdw = 0;

    std::thread m_serverThread;
    void serverFunc();
    std::atomic<bool> m_isRunning{false};

    // Temporary buffer used in serveImage()
    std::string m_buf;

    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr m_jerr;
    JSAMPROW m_row_pointer;  // pointer to start of scanline (row of image)

    uint8_t* m_serveImg = nullptr;
    unsigned long int m_serveLen = 0;  // NOLINT
};
