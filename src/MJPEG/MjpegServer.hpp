//=============================================================================
//File Name: MjpegServer.hpp
//Description: An MJPEG server implementation
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef MJPEG_SERVER_HPP
#define MJPEG_SERVER_HPP

#include "../SFML/System/Clock.hpp"
#include "../SFML/System/Thread.hpp"

#include <iostream>
#include <atomic>
#include <list>

#include <cstdint>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

struct selector_t {
    fd_set allSockets; // set containing all the sockets handles
    fd_set socketsReady; // set containing handles of the sockets that are ready
    unsigned int maxSocket; // maximum socket handle
};

class MjpegServer {
public:
    MjpegServer( unsigned short port );
    virtual ~MjpegServer();

    void start();
    void stop();

    void serveImage( uint8_t* image , size_t size );

private:
    struct selector_t m_clientSelector;
    std::list<unsigned int> m_clientSockets;

    unsigned int m_listenSock;
    unsigned short m_port;

    sf::Thread m_serverThread;
    void serverFunc();
    std::atomic<bool> m_isRunning;
};

#endif // MJPEG_SERVER_HPP
