//=============================================================================
//File Name: MjpegServer.cpp
//Description: An MJPEG server implementation
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <sstream>
#include <cstdlib>
#include <cstring>
#include "MjpegServer.hpp"
#include "mjpeg_sleep.h"

MjpegServer::MjpegServer( unsigned short port ) :
        m_listenSock( INVALID_SOCKET ) ,
        m_port( port ) ,
        m_isRunning( false ) {
    // Clear selector
    FD_ZERO(&m_clientSelector.allSockets);
    FD_ZERO(&m_clientSelector.socketsReady);
    m_clientSelector.maxSocket = 0;

    m_serveImg = NULL;
    m_serveLen = 0;

    // Set up the error handler
    m_cinfo.err = jpeg_std_error( &m_jerr );

    // Initialize the JPEG compression object
    jpeg_create_compress( &m_cinfo );

    /* First we supply a description of the input image. Four fields of the
     * cinfo struct must be filled in:
     */
    m_cinfo.image_width = 320;             // image width, in pixels
    m_cinfo.image_height = 240;            // image height, in pixels
    m_cinfo.input_components = 3;          // # of color components per pixel
    m_cinfo.in_color_space = JCS_EXT_BGR;  // colorspace of input image

    // Use the library's routine to set default compression parameters
    jpeg_set_defaults( &m_cinfo );

    m_row_pointer = NULL;
}

MjpegServer::~MjpegServer() {
    stop();

    jpeg_destroy_compress( &m_cinfo );
    std::free( m_serveImg );
}

void MjpegServer::start() {
    if ( !m_isRunning ) {
        m_listenSock = socket(AF_INET, SOCK_STREAM, 0);

        if ( mjpeg_sck_valid(m_listenSock) ) {
            mjpeg_sck_setnonblocking(m_listenSock, 0);

            // Disable the Nagle algorithm (ie. removes buffering of TCP packets)
            int yes = 1;
            if (setsockopt(m_listenSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&yes), sizeof(yes)) == -1) {
                mjpeg_sck_close(m_listenSock);
                std::cout << "MjpegServer: failed to remove TCP buffering\n";
                return; // Failed to remove buffering
            }
        }
        else {
            std::cout << "MjpegServer: failed to create listener socket\n";
            return; // Failed to create socket
        }

        // Bind the socket to the specified port
        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_family      = AF_INET;
        address.sin_port        = htons(m_port);

        if (bind(m_listenSock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
            mjpeg_sck_close(m_listenSock);
            std::cout << "MjpegServer: failed to bind socket to port\n";
            return; // Failed to bind socket to port
        }

        // Listen to the bound port
        if (listen(m_listenSock, 0) == -1) {
            mjpeg_sck_close(m_listenSock);
            std::cout << "MjpegServer: failed to listen to port " << m_port << "\n";
            return; // Failed to listen to port
        }

        std::cout << "Started listening on " << m_port << "\n";

        // Add listening socket to selector
        FD_SET(m_listenSock, &m_clientSelector.allSockets);
        if ( m_listenSock > m_clientSelector.maxSocket) {
            m_clientSelector.maxSocket = m_listenSock;
        }

        m_isRunning = true;
        if ( mjpeg_thread_create( &m_serverThread , &MjpegServer::serverFunc , this ) == -1 ) {
            m_isRunning = false;
        }
    }
}

void MjpegServer::stop() {
    if ( m_isRunning ) {
        m_isRunning = false;
        mjpeg_thread_join( &m_serverThread , NULL );

        // Clear selector
        FD_ZERO(&m_clientSelector.allSockets);
        FD_ZERO(&m_clientSelector.socketsReady);
        m_clientSelector.maxSocket = 0;

        mjpeg_sck_close(m_listenSock);

        // Close and disconnect client sockets
        for ( std::list<mjpeg_socket_t>::iterator i = m_clientSockets.begin() ; i != m_clientSockets.end() ; i++ ) {
            mjpeg_sck_close(*i);
        }

        m_clientSockets.clear();
    }
}

void MjpegServer::serveImage( uint8_t* image , unsigned int width , unsigned int height ) {
    /* Free output buffer for JPEG compression because libjpeg leaks
     * memory from jpeg_mem_dest() if it's not NULL (see libjpeg's
     * jdatadst.c line 242 for the function)
     */
    std::free( m_serveImg );
    m_serveImg = NULL;

    // Specify data destination (e.g. memory buffer)
    jpeg_mem_dest( &m_cinfo , &m_serveImg , &m_serveLen );

    /* ===== Convert RGB image to JPEG ===== */
    m_cinfo.image_width = width;
    m_cinfo.image_height = height;

    // Set any non-default parameters
    jpeg_set_quality( &m_cinfo , 100 , TRUE /* limit to baseline-JPEG values */);

    // TRUE ensures that we will write a complete interchange-JPEG file
    jpeg_start_compress( &m_cinfo , TRUE );

    /* while scan lines remain to be written...
     * Here we use the library's state variable cinfo.next_scanline as
     * the loop counter, so that we don't have to keep track ourselves.
     */
    while ( m_cinfo.next_scanline < m_cinfo.image_height ) {
        m_row_pointer = image + m_cinfo.next_scanline * m_cinfo.image_width *
                m_cinfo.input_components;
        (void) jpeg_write_scanlines( &m_cinfo , &m_row_pointer , 1 );
    }

    jpeg_finish_compress( &m_cinfo );
    /* ===================================== */

    // Send JPEG to all clients
    for ( std::list<mjpeg_socket_t>::iterator i = m_clientSockets.begin() ; i != m_clientSockets.end() ; i++ ) {
        std::string imgFrame = "--myboundary\r\n";
        imgFrame += "Content-Type: image/jpeg\r\n";
        imgFrame += "Content-Length: ";

        std::stringstream ss;
        ss << m_serveLen;

        imgFrame += ss.str(); // Add image size
        imgFrame += "\r\n\r\n";

        bool didSend = true;
        char buffer[imgFrame.length() + m_serveLen + 2];
        std::memcpy( buffer , imgFrame.c_str() , imgFrame.length() );
        std::memcpy( buffer + imgFrame.length() , m_serveImg , m_serveLen );
        std::memcpy( buffer + imgFrame.length() + m_serveLen , "\r\n" , 2 );

        // Loop until every byte has been sent
        int sent = 0;
        int sizeToSend = imgFrame.length() + m_serveLen + 2;
        for (int length = 0; length < sizeToSend; length += sent) {
            // Send a chunk of data
            sent = send(*i, buffer + length, sizeToSend - length, 0);

            // Check for errors
            if (sent < 0) {
                didSend = false;
                break; // Failed to send
            }
        }

        if ( !didSend ) {
            // Close dead socket and remove it from the selector
            mjpeg_sck_close(*i);
            FD_CLR(*i, &m_clientSelector.allSockets);
            FD_CLR(*i, &m_clientSelector.socketsReady);

            // Remove socket from the list of clients
            m_clientSockets.erase( i );

            break; // Exit the loop since we lost the iterator
        }
    }
}

void* MjpegServer::serverFunc( void* obj ) {
    MjpegServer* objPtr = static_cast<MjpegServer*>( obj );

    char packet[256];
    unsigned int recvSize = 0;

    while ( objPtr->m_isRunning ) {
        // Set up the timeout
        timeval time;
        time.tv_sec  = 0;
        time.tv_usec = 1;

        // Initialize the set that will contain the sockets that are ready
        objPtr->m_clientSelector.socketsReady = objPtr->m_clientSelector.allSockets;

        // Wait until one of the sockets is ready for reading, or timeout is reached
        int count = select(objPtr->m_clientSelector.maxSocket + 1, &objPtr->m_clientSelector.socketsReady, NULL, NULL, &time);

        // Call wrapper for select so 'isReady' call works
        if ( count > 0 ) {
            // If listener is ready
            if ( FD_ISSET(objPtr->m_listenSock, &objPtr->m_clientSelector.socketsReady) != 0 ) {
                // Accept a new connection
                sockaddr_in acceptAddr;
                socklen_t length = sizeof(acceptAddr);
                mjpeg_socket_t newClient = accept(objPtr->m_listenSock, reinterpret_cast<sockaddr*>(&acceptAddr), &length);

                // Initialize new socket and add it to the selector
                if ( mjpeg_sck_valid(newClient) ) {
                    mjpeg_sck_setnonblocking(newClient, 0);

                    // Disable the Nagle algorithm (ie. removes buffering of TCP packets)
                    int yes = 1;
                    if (setsockopt(newClient, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&yes), sizeof(yes)) == -1) {
                        mjpeg_sck_close(newClient);
                        break; // Failed remove buffering
                    }

                    // Add socket to selector
                    objPtr->m_clientSockets.push_front( newClient );
                    FD_SET(newClient, &objPtr->m_clientSelector.allSockets);
                    if ( newClient > objPtr->m_clientSelector.maxSocket) {
                        objPtr->m_clientSelector.maxSocket = newClient;
                    }
                }
            }
            else {
                // Check if sockets are requesting data stream
                std::list<mjpeg_socket_t>::iterator i = objPtr->m_clientSockets.begin();
                while ( i != objPtr->m_clientSockets.end() ) {
                    // If current client is ready
                    if ( FD_ISSET(*i, &objPtr->m_clientSelector.socketsReady) != 0 ) {
                        // Receive a chunk of bytes
                        recvSize = recv(*i, packet, 256, 0);

                        if ( recvSize > 0 ) {
                            /* Parse request to determine the right MJPEG stream to send them
                             * It should be "GET %s HTTP/1.0\r\n\r\n"
                             */
                            char* tok = std::strtok( packet , " " );
                            if ( std::strncmp( tok , "GET" , 3 ) == 0 ) {
                                // Get request path
                                tok = std::strtok( NULL , " " );

                                // TODO Finish parsing

                                std::string ack = "HTTP/1.0 200 OK\r\n"
                                        "Cache-Control: no-cache\r\n"
                                        "Connection: close\r\n"
                                        "Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n\r\n";

                                // Loop until every byte has been sent
                                int sent = 0;
                                for ( unsigned int pos = 0 ; pos < ack.length() ; pos += sent ) {
                                    // Send a chunk of data
                                    sent = send(*i, ack.c_str() + pos, ack.length() - pos, 0);

                                    // Check for errors
                                    if ( sent < 0 ) {
                                        break; // Failed to send
                                    }
                                }
                            }
                        }

                        // If socket disconnected or malfunctioned, remove it
                        else {
                            // Close dead socket and remove it from the selector
                            mjpeg_sck_close(*i);
                            FD_CLR(*i, &objPtr->m_clientSelector.allSockets);
                            FD_CLR(*i, &objPtr->m_clientSelector.socketsReady);

                            // Remove socket from the list of clients
                            i = objPtr->m_clientSockets.erase( i );

                            continue;
                        }
                    }

                    i++;
                }
            }
        }

        mjpeg_sleep( 100 );
    }

    return NULL;
}

#ifdef _WIN32
struct SocketInitializer
{
    SocketInitializer()
    {
        WSADATA init;
        WSAStartup(MAKEWORD(2, 2), &init);
    }

    ~SocketInitializer()
    {
        WSACleanup();
    }
};

SocketInitializer globalInitializer;
#endif
