//=============================================================================
//File Name: MjpegServer.hpp
//Description: An MJPEG server implementation
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef MJPEG_SERVER_HPP
#define MJPEG_SERVER_HPP

#include "mjpeg_thread.h"
#include "mjpeg_sck.h"

#include <iostream>
#include <atomic>
#include <list>
#include <cstdint>

#include <jpeglib.h>

struct selector_t {
    fd_set allSockets; // set containing all the sockets handles
    fd_set socketsReady; // set containing handles of the sockets that are ready
    mjpeg_socket_t maxSocket; // maximum socket handle
};

class MjpegServer {
public:
    MjpegServer( unsigned short port );
    virtual ~MjpegServer();

    void start();
    void stop();

    // Converts BGR image to JPEG before serving it
    void serveImage( uint8_t* image , unsigned int width , unsigned int height );

private:
    struct selector_t m_clientSelector;
    std::list<mjpeg_socket_t> m_clientSockets;

    mjpeg_socket_t m_listenSock;
    unsigned short m_port;

    mjpeg_thread_t m_serverThread;
    static void* serverFunc( void* obj );
    std::atomic<bool> m_isRunning;

    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr m_jerr;
    JSAMPROW m_row_pointer; // pointer to start of scanline (row of image)

    uint8_t* m_serveImg;
    unsigned long int m_serveLen;
};

#endif // MJPEG_SERVER_HPP
