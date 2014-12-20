//=============================================================================
//File Name: MjpegClient.cpp
//Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "../Util.hpp"
#include "MjpegClient.hpp"

#include "stb_image.h"
#include "stb_image_write.h"

#include <sys/types.h>

#include <iostream>
#include <system_error>
#include <algorithm>
#include <list>
#include <cstring>

// Convert a string to lower case
std::string toLower( std::string str ) {
    for ( auto i : str ) {
        i = std::tolower(i);
    }
    return str;
}

MjpegClient::MjpegClient( const std::string& hostName , unsigned short port ,
        const std::string& requestPath
        ) :
        m_hostName( hostName ) ,
        m_port( port ) ,
        m_requestPath( requestPath ) ,

        m_pxlBuf( NULL ) ,
        m_imgWidth( 0 ) ,
        m_imgHeight( 0 ) ,

        m_extBuf( NULL ) ,
        m_extWidth( 0 ) ,
        m_extHeight( 0 ) ,

        m_recvThread( nullptr ) ,
        m_stopReceive( true ) ,
        m_cancelfdr( 0 ) ,
        m_cancelfdw( 0 ) ,
        m_sd( INVALID_SOCKET )
{

}

MjpegClient::~MjpegClient() {
    stop();

    delete[] m_pxlBuf;
    delete[] m_extBuf;
}

void MjpegClient::start() {
    if ( !isStreaming() ) { // if stream is closed, reopen it
        mjpeg_socket_t pipefd[2];

        /* Create a pipe that, when written to, causes any operation in the
         * mjpegrx thread currently blocking to be cancelled.
         */
        if ( mjpeg_pipe( pipefd ) != 0 ) {
            return;
        }
        m_cancelfdr = pipefd[0];
        m_cancelfdw = pipefd[1];

        // Mark the thread as running
        m_stopReceive = false;

        m_recvThread = new std::thread( [this] { MjpegClient::recvFunc(); } );
    }
}

void MjpegClient::stop() {
    if ( isStreaming() ) { // if stream is open, close it
        m_stopReceive = true;

        // Cancel any currently blocking operations
        send( m_cancelfdw , "U" , 1 , 0 );

        // Close the receive thread
        m_recvThread->join();
        delete m_recvThread;

        mjpeg_sck_close( m_cancelfdr );
        mjpeg_sck_close( m_cancelfdw );
    }
}

bool MjpegClient::isStreaming() const {
    return !m_stopReceive;
}

void MjpegClient::saveCurrentImage( const std::string& fileName ) {
    m_imageMutex.lock();

    // Deduce the image type from its extension
    if ( fileName.size() > 3 ) {
        // Extract the extension
        std::string extension = fileName.substr(fileName.size() - 3);

        if ( toLower(extension) == "bmp" ) {
            // BMP format
            stbi_write_bmp( fileName.c_str(), m_imgWidth, m_imgHeight, 4, m_pxlBuf );
        }
        else if ( toLower(extension) == "tga" ) {
            // TGA format
            stbi_write_tga( fileName.c_str(), m_imgWidth, m_imgHeight, 4, m_pxlBuf );
        }
        else if( toLower(extension) == "png" ) {
            // PNG format
            stbi_write_png( fileName.c_str(), m_imgWidth, m_imgHeight, 4, m_pxlBuf, 0 );
        }
        else {
            std::cout << "MjpegClient: failed to save image to '" << fileName << "'\n";
        }
    }

    m_imageMutex.unlock();
}

uint8_t* MjpegClient::getCurrentImage() {
    m_imageMutex.lock();
    m_extMutex.lock();

    if ( m_pxlBuf != NULL ) {
        // If buffer is wrong size, reallocate it
        if ( m_imgWidth != m_extWidth || m_imgHeight != m_extHeight ) {
            if ( m_extBuf != NULL ) {
                delete[] m_extBuf;
            }

            // Allocate new buffer to fit latest image
            m_extBuf = new uint8_t[m_imgWidth * m_imgHeight * 4];
            m_extWidth = m_imgWidth;
            m_extHeight = m_imgHeight;
        }

        std::memcpy( m_extBuf , m_pxlBuf , m_extWidth * m_extHeight * 4 );
    }

    m_extMutex.unlock();
    m_imageMutex.unlock();

    return m_extBuf;
}

unsigned int MjpegClient::getCurrentWidth() {
    m_extMutex.lock();

    unsigned int temp( m_extWidth );

    m_extMutex.unlock();

    return temp;
}

unsigned int MjpegClient::getCurrentHeight() {
    m_extMutex.lock();

    unsigned int temp( m_extHeight );

    m_extMutex.unlock();

    return temp;
}

char* strtok_r_n( char* str , const char* sep , char** last , char* used );

/* Read a byte from sd into (*buf)+*bufpos . If afterwards,
   *bufpos == (*bufsize)-1, reallocate the buffer as 1024
   bytes larger, updating *bufsize . This function blocks
   until either a byte is received, or cancelfd becomes
   ready for reading. */
int mjpeg_rxbyte( char** buf , int* bufpos , int* bufsize , int sd , int cancelfd ) {
    int bytesread;

    bytesread = mjpeg_sck_recv( sd , (*buf) + *bufpos , 1 , cancelfd );
    if ( bytesread == -1 ) return -1;
    if ( bytesread != 1 ) return 1;
    (*bufpos)++;

    if(*bufpos == (*bufsize)-1){
        *bufsize += 1024;
        *buf = static_cast<char*>( realloc(*buf, *bufsize) );
    }

    return 0;
}

// Read data up until the character sequence "\r\n\r\n" is received.
int mjpeg_rxheaders( char** buf_out , int* bufsize_out , int sd , int cancelfd ) {
    int allocsize = 1024;
    int bufpos = 0;
    char* buf = static_cast<char*>( malloc(allocsize) );

    while ( 1 ) {
        if ( mjpeg_rxbyte( &buf , &bufpos , &allocsize , sd , cancelfd ) != 0 ) {
            free( buf );
            return -1;
        }
        if ( bufpos >= 4 && buf[bufpos-4] == '\r' && buf[bufpos-3] == '\n' &&
            buf[bufpos-2] == '\r' && buf[bufpos-1] == '\n'){
            break;
        }
    }
    buf[bufpos] = '\0';

    *buf_out = buf;
    *bufsize_out = bufpos;

    return 0;
}

/* mjpeg_sck_recv() blocks until either len bytes of data have
   been read into buf, or cancelfd becomes ready for reading.
   If either len bytes are read, or cancelfd becomes ready for
   reading, the number of bytes received is returned. On error,
   -1 is returned, and errno is set appropriately. */
int mjpeg_sck_recv( int sockfd , void* buf , size_t len , int cancelfd ) {
    int error;
    size_t nread;
    fd_set readfds;
    fd_set exceptfds;

    nread = 0;
    while ( nread < len ) {
        FD_ZERO( &readfds );
        FD_ZERO( &exceptfds );

        // Set the sockets into the fd_set s
        FD_SET( sockfd , &readfds );
        FD_SET( sockfd , &exceptfds );
        if( cancelfd ) {
            FD_SET( cancelfd , &readfds );
            FD_SET( cancelfd , &exceptfds );
        }

        error = select( std::max(sockfd, cancelfd) + 1 , &readfds , NULL , &exceptfds , NULL );
        if( error == -1 ) {
            return -1;
        }

        // If an exception occurred with either one, return error.
        if ( ( cancelfd && FD_ISSET(cancelfd, &exceptfds)) ||
                FD_ISSET(sockfd, &exceptfds) ) {
            return -1;
        }

        /* If cancelfd is ready for reading, return now with what we have read
         * so far.
         */
        if ( cancelfd && FD_ISSET(cancelfd, &readfds) ) {
            return nread;
        }

        // Otherwise, read some more.
        error = recv( sockfd , static_cast<char*>(buf) + nread , len - nread , 0 );
        if ( error < 1 ) {
            return -1;
        }
        nread += error;
    }

    return nread;
}

/* Searches string 'str' for any of the characters in 'c', then returns a
 * pointer to the first occurrence of any one of those characters, NULL if
 * none are found
 */
char* strchrs( char* str , const char* c ) {
    for ( char* t = str ; *t != '\0' ; t++ ) {
        for ( const char* ct = c ; *ct != '\0' ; ct++ ) {
            if ( *t == *ct ) {
                return t;
            }
        }
    }

    return NULL;
}

/* A slightly modified version of strtok_r. When the function encounters a
   character in the separator list, that character is set into *used before
   the token is returned. This allows the caller to determine which separator
   character preceded the returned token. */
char* strtok_r_n( char* str , const char* sep , char** last , char* used ) {
    char* strsep;
    char* ret;

    if ( str != NULL ) {
        *last = str;
    }

    strsep = strchrs( *last , sep );
    if ( strsep == NULL ) {
        return NULL;
    }
    if ( used != NULL ) {
        *used = *strsep;
    }
    *strsep = '\0';

    ret = *last;
    *last = strsep+1;

    return ret;
}

/* Processes the HTTP response headers, separating them into key-value
   pairs. These are then stored in a linked list. The "header" argument
   should point to a block of HTTP response headers in the standard ':'
   and '\n' separated format. The key is the text on a line before the
   ':', the value is the text after the ':', but before the '\n'. Any
   line without a ':' is ignored. a pointer to the first element in a
   linked list is returned. */
std::list<std::pair<char*,char*>> mjpeg_process_header( char* header ) {
    char* strtoksave;
    std::list<std::pair<char*,char*>> list;
    char* key;
    char* value;
    char used;

    header = strdup( header );
    if ( header == NULL ) {
        return list;
    }

    key = strtok_r_n( header , ":\n" , &strtoksave , &used );
    if ( key == NULL ) {
        return list;
    }

    while ( 1 ) { // we break out inside
        // if no ':' exists on the line, ignore it
        if ( used == '\n' ) {
            key = strtok_r_n(
                NULL ,
                ":\n" ,
                &strtoksave ,
                &used );
            if ( key == NULL ) {
                break;
            }
            continue;
        }

        // create a linked list element
        list.push_back( std::pair<char*,char*>() );

        // save off the key
        list.back().first = strdup( key );

        // get the value
        value = strtok_r_n( NULL , "\n" , &strtoksave , NULL );
        if ( value == NULL ) {
            list.back().second = strdup( "" );
            break;
        }
        value++;
        if ( value[strlen(value)-1] == '\r' ) {
            value[strlen(value)-1] = '\0';
        }
        list.back().second = strdup( value );

        /* get the key for next loop */
        key = strtok_r_n( NULL , ":\n" , &strtoksave , &used );
        if ( key == NULL ) {
            break;
        }
    }

    free( header );

    return list;
}

/* mjpeg_freelist() frees a key/value pair list generated by
   mjpeg_process_header() . */
void mjpeg_freelist( std::list<std::pair<char*,char*>>& list ){
    for ( auto i : list ) {
        free( i.first );
        free( i.second );
    }
}

/* Return the data in the specified list that corresponds
   to the specified key. */
char* mjpeg_getvalue( std::list<std::pair<char*,char*>>& list , const char* key ) {
    for ( auto i : list ) {
        // Check for matching key
        if ( strcmp( i.first , key ) == 0 ) {
            return i.second;
        }
    }

    // Return NULL if no match found
    return NULL;
}

void MjpegClient::recvFunc() {
    startCallback();

    mjpeg_socket_t sd;

    char* asciisize;
    int datasize;
    char* buf;
    char tmp[256];

    char* headerbuf;
    int headerbufsize;
    std::list<std::pair<char*,char*>> headerlist;

    int bytesread;

    /* Connect to the remote host. */
    sd = mjpeg_sck_connect( m_hostName.c_str() , m_port , m_cancelfdr );
    if ( !mjpeg_sck_valid( sd ) ) {
        std::cerr << "mjpegrx: Connection failed\n";
        m_stopReceive = true;
        stopCallback();

        return;
    }
    m_sd = sd;

    // Send the HTTP request.
    snprintf( tmp , 255 , "GET %s HTTP/1.0\r\n\r\n" , m_requestPath.c_str() );
    send( sd , tmp , strlen(tmp) , 0 );
    std::cout << tmp;

    while( !m_stopReceive ) {
        // Read and parse incoming HTTP response headers.
        if ( mjpeg_rxheaders( &headerbuf , &headerbufsize , sd , m_cancelfdr ) == -1 ) {
            std::cerr << "mjpegrx: recv(2) failed\n";
            break;
        }
        headerlist = mjpeg_process_header( headerbuf );
        free( headerbuf );
        if ( headerlist.size() == 0 ) {
            break;
        }

        /* Read the Content-Length header to determine the length of data to
         * read.
         */
        asciisize = mjpeg_getvalue(
            headerlist ,
            "Content-Length" );

        if ( asciisize == NULL ) {
            mjpeg_freelist( headerlist );
            continue;
        }

        datasize = atoi( asciisize );
        mjpeg_freelist( headerlist );

        // Read the JPEG image data.
        buf = static_cast<char*>( malloc(datasize) );
        bytesread = mjpeg_sck_recv( sd , buf , datasize , m_cancelfdr );
        if ( bytesread != datasize ) {
            free( buf );
            std::cerr << "mjpegrx: recv(2) failed\n";
            break;
        }

        // Load the image received (converts from JPEG to pixel array)
        int width, height, channels;
        uint8_t* ptr = stbi_load_from_memory(reinterpret_cast<unsigned char*>(buf), datasize, &width, &height, &channels, STBI_rgb_alpha);

        if ( ptr && width && height ) {
            m_imageMutex.lock();

            // Free old buffer and store new one created by stbi_load_from_memory()
            delete[] m_pxlBuf;

            m_pxlBuf = ptr;

            m_imgWidth = width;
            m_imgHeight = height;

            m_imageMutex.unlock();

            newImageCallback( buf , datasize );
        }
        else {
            std::cout << "MjpegClient: image failed to load: " << stbi_failure_reason() << "\n";
        }

        free( buf );
    }

    // The loop has exited. We should now clean up and exit the thread.
    mjpeg_sck_close( sd );

    stopCallback();
}
