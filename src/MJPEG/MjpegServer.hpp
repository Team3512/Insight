// =============================================================================
// Description: An MJPEG server implementation
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#ifndef MJPEG_SERVER_HPP
#define MJPEG_SERVER_HPP

#include <atomic>
#include <list>
#include <mutex>
#include <thread>
#include <cstdint>

#include <jpeglib.h>
#include "mjpeg_sck_selector.hpp"

class MjpegServer {
public:
    MjpegServer(unsigned short port);
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
    unsigned short m_port;

    mjpeg_socket_t m_cancelfdr = 0;
    mjpeg_socket_t m_cancelfdw = 0;

    std::thread m_serverThread;
    void serverFunc();
    std::atomic<bool> m_isRunning{false};

    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr m_jerr;
    JSAMPROW m_row_pointer; // pointer to start of scanline (row of image)

    uint8_t* m_serveImg = nullptr;
    unsigned long int m_serveLen = 0;
};

#endif // MJPEG_SERVER_HPP

