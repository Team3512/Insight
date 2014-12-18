//=============================================================================
//File Name: MjpegServer.cpp
//Description: An MJPEG server implementation
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include "MjpegServer.hpp"

MjpegServer::MjpegServer( unsigned short port ) :
        m_listenSock( INVALID_SOCKET ) ,
        m_port( port ) ,
        m_cancelfdr( 0 ) ,
        m_cancelfdw( 0 ) ,
        m_isRunning( false ) {
    m_serverThread = nullptr;

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

    // Set any non-default parameters
    jpeg_set_quality( &m_cinfo , 100 , TRUE /* limit to baseline-JPEG values */);

    m_row_pointer = NULL;
}

MjpegServer::~MjpegServer() {
    stop();

    jpeg_destroy_compress( &m_cinfo );
    std::free( m_serveImg );
}

void MjpegServer::start() {
    if ( !m_isRunning ) {
        m_listenSock = socket( AF_INET , SOCK_STREAM , 0 );

        if ( mjpeg_sck_valid(m_listenSock) ) {
            mjpeg_sck_setnonblocking( m_listenSock , 0 );

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

        mjpeg_socket_t pipefd[2];

        /* Create a pipe that, when written to, causes any operation in the
         * mjpegrx thread currently blocking to be cancelled.
         */
        if ( mjpeg_pipe( pipefd ) != 0 ) {
            mjpeg_sck_close( m_listenSock );
            return;
        }
        m_cancelfdr = pipefd[0];
        m_cancelfdw = pipefd[1];

        // Add reading end of socketpair to selector
        m_clientSelector.addSocket( m_cancelfdr , mjpeg_sck_selector::read | mjpeg_sck_selector::except );

        // Add listening socket to selector
        m_clientSelector.addSocket( m_listenSock , mjpeg_sck_selector::read | mjpeg_sck_selector::except );

        m_isRunning = true;
        m_serverThread = new std::thread( MjpegServer::serverFunc , this );
    }
}

void MjpegServer::stop() {
    if ( m_isRunning ) {
        m_isRunning = false;

        // Cancel any currently blocking operations
        send( m_cancelfdw , "U" , 1 , 0 );

        m_serverThread->join();
        delete m_serverThread;

        mjpeg_sck_close( m_cancelfdr );
        mjpeg_sck_close( m_cancelfdw );
        mjpeg_sck_close( m_listenSock );

        // Close and disconnect client sockets
        for ( auto i : m_clientSockets ) {
            mjpeg_sck_close(i);
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

    /* Don't bother making the MJPEG frame if there are no clients to which to
     * send it.
     */
    if ( m_clientSockets.size() == 0 ) {
        return;
    }

    /* ===== Prepare MJPEG frame ===== */
    std::string imgFrame = "--myboundary\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: ";

    std::stringstream ss;
    ss << m_serveLen;

    imgFrame += ss.str(); // Add image size
    imgFrame += "\r\n\r\n";

    char buffer[imgFrame.length() + m_serveLen + 2];
    std::memcpy( buffer , imgFrame.c_str() , imgFrame.length() );
    std::memcpy( buffer + imgFrame.length() , m_serveImg , m_serveLen );
    std::memcpy( buffer + imgFrame.length() + m_serveLen , "\r\n" , 2 );
    /* =============================== */

    // Send JPEG to all clients
    for ( auto i = m_clientSockets.begin() ; i != m_clientSockets.end() ; i++ ) {
        // Loop until every byte has been sent
        int sent = 0;
        int sizeToSend = imgFrame.length() + m_serveLen + 2;
        for ( int length = 0 ; length < sizeToSend ; length += sent ) {
            // Send a chunk of data
            sent = send( *i , buffer + length , sizeToSend - length , 0 );

            // Check for errors
            if ( sent < 0 ) {
                // Close dead socket and remove it from the selector
                mjpeg_sck_close( *i );
                m_clientSelector.removeSocket( *i ,
                        mjpeg_sck_selector::read | mjpeg_sck_selector::except );

                // Remove socket from the list of clients
                i = m_clientSockets.erase( i );

                break; // Failed to send
            }
        }
    }
}

void MjpegServer::serverFunc() {
    char packet[256];
    unsigned int recvSize = 0;

    while ( m_isRunning ) {
        // Wait until one of the sockets is ready for reading, or timeout is reached
        int count = m_clientSelector.select( NULL );

        if ( count > 0 ) {
            // If listener is ready to be read from
            if ( m_clientSelector.isReady( m_listenSock , mjpeg_sck_selector::read ) ) {
                std::cout << "new client\n";

                // Accept a new connection
                sockaddr_in acceptAddr;
                socklen_t length = sizeof(acceptAddr);
                mjpeg_socket_t newClient = accept(m_listenSock, reinterpret_cast<sockaddr*>(&acceptAddr), &length);

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
                    m_clientSockets.push_front( newClient );
                    m_clientSelector.addSocket( newClient , mjpeg_sck_selector::read | mjpeg_sck_selector::except );
                }
            }

            /* If an exception occurred with the cancel socket or it is ready
             * to be read, return.
             */
            if ( m_cancelfdr &&
                    (m_clientSelector.isReady( m_cancelfdr , mjpeg_sck_selector::except ) ||
                     m_clientSelector.isReady( m_cancelfdr , mjpeg_sck_selector::read )) ) {
                return;
            }

            // Check if sockets are requesting data stream
            for ( auto i = m_clientSockets.begin() ; i != m_clientSockets.end() ; i++ ) {
                // If current client is ready to be read from
                if ( m_clientSelector.isReady( *i , mjpeg_sck_selector::read ) ) {
                    // Receive a chunk of bytes
                    recvSize = recv(*i, packet, 255, 0);
                    packet[recvSize] = '\0';

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
                                sent = send( *i , ack.c_str() + pos , ack.length() - pos , 0 );

                                // Check for errors
                                if ( sent < 0 ) {
                                    // Close dead socket and remove it from the selector
                                    mjpeg_sck_close( *i );
                                    m_clientSelector.removeSocket( *i ,
                                            mjpeg_sck_selector::read | mjpeg_sck_selector::except );

                                    // Remove socket from the list of clients
                                    i = m_clientSockets.erase( i );

                                    break; // Failed to send
                                }
                            }
                        }
                    }

                    // If socket disconnected or malfunctioned, remove it
                    else {
                        // Close dead socket and remove it from the selector
                        mjpeg_sck_close( *i );
                        m_clientSelector.removeSocket( *i ,
                                mjpeg_sck_selector::read | mjpeg_sck_selector::except );

                        // Remove socket from the list of clients
                        i = m_clientSockets.erase( i );

                        continue;
                    }
                }

                if ( m_clientSelector.isReady( *i , mjpeg_sck_selector::except ) ) {
                    // Close dead socket and remove it from the selector
                    mjpeg_sck_close( *i );
                    m_clientSelector.removeSocket( *i ,
                            mjpeg_sck_selector::read | mjpeg_sck_selector::except );

                    // Remove socket from the list of clients
                    i = m_clientSockets.erase( i );

                    continue;
                }
            }
        }
    }
}
