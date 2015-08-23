// =============================================================================
// File Name: MjpegClient.cpp
// Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include "../Util.hpp"
#include "MjpegClient.hpp"
#include "mjpeg_sck_selector.hpp"

#include <QImage>
#include <QString>

#include <sys/types.h>

#include <iostream>
#include <algorithm>
#include <list>
#include <cstring>

// Convert a string to lower case
std::string toLower(std::string str) {
    for (auto i : str) {
        i = std::tolower(i);
    }
    return str;
}

MjpegClient::MjpegClient(const std::string& hostName,
                         unsigned short port,
                         const std::string& requestPath) :
    m_hostName(hostName),
    m_port(port),
    m_requestPath(requestPath) {
    m_cinfo.err = jpeg_std_error(&m_jerr);
    m_cinfo.do_fancy_upsampling = 0;
    m_cinfo.do_block_smoothing = 0;

    jpeg_create_decompress(&m_cinfo);
}

MjpegClient::~MjpegClient() {
    stop();

    jpeg_destroy_decompress(&m_cinfo);

    delete[] m_pxlBuf;
    delete[] m_extBuf;
}

void MjpegClient::start() {
    if (!isStreaming()) { // if stream is closed, reopen it
        mjpeg_socket_t pipefd[2];

        /* Create a pipe that, when written to, causes any operation in the
         * mjpegrx thread currently blocking to be cancelled.
         */
        if (mjpeg_pipe(pipefd) != 0) {
            return;
        }
        m_cancelfdr = pipefd[0];
        m_cancelfdw = pipefd[1];

        // Mark the thread as running
        m_stopReceive = false;

        m_recvThread = std::thread([this] { MjpegClient::recvFunc(); });
    }
}

void MjpegClient::stop() {
    if (isStreaming()) { // if stream is open, close it
        m_stopReceive = true;

        // Cancel any currently blocking operations
        send(m_cancelfdw, "U", 1, 0);

        // Close the receive thread
        m_recvThread.join();

        mjpeg_sck_close(m_cancelfdr);
        mjpeg_sck_close(m_cancelfdw);
    }
}

bool MjpegClient::isStreaming() const {
    return !m_stopReceive;
}

void MjpegClient::saveCurrentImage(const std::string& fileName) {
    m_imageMutex.lock();

    QImage tmp(m_pxlBuf, m_imgWidth, m_imgHeight, QImage::Format_RGB888);
    if (!tmp.save(fileName.c_str())) {
        std::cout << "MjpegClient: failed to save image to '" << fileName <<
            "'\n";
    }

    m_imageMutex.unlock();
}

uint8_t* MjpegClient::getCurrentImage() {
    m_imageMutex.lock();
    m_extMutex.lock();

    if (m_pxlBuf != NULL) {
        // If buffer is wrong size, reallocate it
        if (m_imgWidth != m_extWidth || m_imgHeight != m_extHeight) {
            if (m_extBuf != NULL) {
                delete[] m_extBuf;
            }

            // Allocate new buffer to fit latest image
            m_extBuf = new uint8_t[m_imgWidth * m_imgHeight * m_imgChannels];
            m_extWidth = m_imgWidth;
            m_extHeight = m_imgHeight;
        }

        std::memcpy(m_extBuf, m_pxlBuf,
                    m_extWidth * m_extHeight * m_imgChannels);
    }

    m_extMutex.unlock();
    m_imageMutex.unlock();

    return m_extBuf;
}

unsigned int MjpegClient::getCurrentWidth() const {
    m_extMutex.lock();

    unsigned int temp(m_extWidth);

    m_extMutex.unlock();

    return temp;
}

unsigned int MjpegClient::getCurrentHeight() const {
    m_extMutex.lock();

    unsigned int temp(m_extHeight);

    m_extMutex.unlock();

    return temp;
}

uint8_t* MjpegClient::jpeg_load_from_memory(uint8_t* buffer, int len) {
    jpeg_mem_src(&m_cinfo, buffer, len);

    if (jpeg_read_header(&m_cinfo, TRUE) != JPEG_HEADER_OK) {
        return NULL;
    }

    jpeg_start_decompress(&m_cinfo);

    int m_row_stride = m_cinfo.output_width * m_cinfo.output_components;

    JSAMPARRAY samp = (*m_cinfo.mem->alloc_sarray)
                          ((j_common_ptr) & m_cinfo, JPOOL_IMAGE, m_row_stride,
                          1);
    uint8_t* imageBuf = new uint8_t[m_row_stride * m_cinfo.output_height];

    m_imgWidth = m_cinfo.output_width;
    m_imgHeight = m_cinfo.output_height;
    m_imgChannels = m_cinfo.output_components;

    for (int outpos = 0; m_cinfo.output_scanline < m_cinfo.output_height;
         outpos += m_row_stride) {
        jpeg_read_scanlines(&m_cinfo, samp, 1);

        /* Assume put_scanline_someplace wants a pointer and sample count. */
        std::memcpy(imageBuf + outpos, samp[0], m_row_stride);
    }

    jpeg_finish_decompress(&m_cinfo);

    return imageBuf;
}

char* strtok_r_n(char* str, const char* sep, char** last, char* used);

/* Read a byte from sd into (*buf)+*bufpos . If afterwards,
 * bufpos == (*bufsize)-1, reallocate the buffer as 1024
 *  bytes larger, updating *bufsize . This function blocks
 *  until either a byte is received, or cancelfd becomes
 *  ready for reading. */
int mjpeg_rxbyte(char** buf, int* bufpos, int* bufsize, int sd, int cancelfd) {
    int bytesread;

    bytesread = mjpeg_sck_recv(sd, (*buf) + *bufpos, 1, cancelfd);
    if (bytesread == -1) {
        return -1;
    }
    if (bytesread != 1) {
        return 1;
    }
    (*bufpos)++;

    if (*bufpos == (*bufsize) - 1) {
        *bufsize += 1024;
        *buf = static_cast<char*>(realloc(*buf, *bufsize));
    }

    return 0;
}

// Read data up until the character sequence "\r\n\r\n" is received.
int mjpeg_rxheaders(char** buf_out, int* bufsize_out, int sd, int cancelfd) {
    int allocsize = 1024;
    int bufpos = 0;
    char* buf = static_cast<char*>(malloc(allocsize));

    do {
        if (mjpeg_rxbyte(&buf, &bufpos, &allocsize, sd, cancelfd) != 0) {
            free(buf);
            return -1;
        }
    } while (!(bufpos >= 4 && buf[bufpos - 4] == '\r' &&
               buf[bufpos - 3] == '\n' &&
               buf[bufpos - 2] == '\r' && buf[bufpos - 1] == '\n'));
    buf[bufpos] = '\0';

    *buf_out = buf;
    *bufsize_out = bufpos;

    return 0;
}

/* mjpeg_sck_recv() blocks until either len bytes of data have
 *  been read into buf, or cancelfd becomes ready for reading.
 *  If either len bytes are read, or cancelfd becomes ready for
 *  reading, the number of bytes received is returned. On error,
 *  -1 is returned, and errno is set appropriately. */
int mjpeg_sck_recv(int sockfd, void* buf, size_t len, int cancelfd) {
    int error;
    size_t nread;

    mjpeg_sck_selector selector;

    nread = 0;
    while (nread < len) {
        selector.zero(mjpeg_sck_selector::read | mjpeg_sck_selector::except);

        // Set the sockets into the fd_set s
        selector.addSocket(sockfd,
                           mjpeg_sck_selector::read |
                           mjpeg_sck_selector::except);
        if (cancelfd) {
            selector.addSocket(cancelfd,
                               mjpeg_sck_selector::read |
                               mjpeg_sck_selector::except);
        }

        error = selector.select(NULL);

        if (error == -1) {
            return -1;
        }

        // If an exception occurred with either one, return error.
        if ((cancelfd &&
             selector.isReady(cancelfd, mjpeg_sck_selector::except)) ||
            selector.isReady(sockfd, mjpeg_sck_selector::except)) {
            return -1;
        }

        /* If cancelfd is ready for reading, return now with what we have read
         * so far.
         */
        if (cancelfd && selector.isReady(cancelfd, mjpeg_sck_selector::read)) {
            return nread;
        }

        // Otherwise, read some more.
        error = recv(sockfd, static_cast<char*>(buf) + nread, len - nread, 0);
        if (error < 1) {
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
char* strchrs(char* str, const char* c) {
    for (char* t = str; *t != '\0'; t++) {
        for (const char* ct = c; *ct != '\0'; ct++) {
            if (*t == *ct) {
                return t;
            }
        }
    }

    return NULL;
}

/* A slightly modified version of strtok_r. When the function encounters a
 *  character in the separator list, that character is set into *used before
 *  the token is returned. This allows the caller to determine which separator
 *  character preceded the returned token. */
char* strtok_r_n(char* str, const char* sep, char** last, char* used) {
    char* strsep;
    char* ret;

    if (str != NULL) {
        *last = str;
    }

    strsep = strchrs(*last, sep);
    if (strsep == NULL) {
        return NULL;
    }
    if (used != NULL) {
        *used = *strsep;
    }
    *strsep = '\0';

    ret = *last;
    *last = strsep + 1;

    return ret;
}

/* Processes the HTTP response headers, separating them into key-value
 *  pairs. These are then stored in a linked list. The "header" argument
 *  should point to a block of HTTP response headers in the standard ':'
 *  and '\n' separated format. The key is the text on a line before the
 *  ':', the value is the text after the ':', but before the '\n'. Any
 *  line without a ':' is ignored. a pointer to the first element in a
 *  linked list is returned. */
std::list<std::pair<char*, char*>> mjpeg_process_header(char* header) {
    char* strtoksave;
    std::list<std::pair<char*, char*>> list;
    char* key;
    char* value;
    char used;

    header = strdup(header);
    if (header == NULL) {
        return list;
    }

    key = strtok_r_n(header, ":\n", &strtoksave, &used);
    if (key == NULL) {
        return list;
    }

    while (1) { // we break out inside
        // if no ':' exists on the line, ignore it
        if (used == '\n') {
            key = strtok_r_n(
                NULL,
                ":\n",
                &strtoksave,
                &used);
            if (key == NULL) {
                break;
            }
            continue;
        }

        // create a linked list element
        list.push_back(std::pair<char*, char*>());

        // save off the key
        list.back().first = strdup(key);

        // get the value
        value = strtok_r_n(NULL, "\n", &strtoksave, NULL);
        if (value == NULL) {
            list.back().second = strdup("");
            break;
        }
        value++;
        if (value[strlen(value) - 1] == '\r') {
            value[strlen(value) - 1] = '\0';
        }
        list.back().second = strdup(value);

        /* get the key for next loop */
        key = strtok_r_n(NULL, ":\n", &strtoksave, &used);
        if (key == NULL) {
            break;
        }
    }

    free(header);

    return list;
}

/* mjpeg_freelist() frees a key/value pair list generated by
 *  mjpeg_process_header() . */
void mjpeg_freelist(std::list<std::pair<char*, char*>>& list) {
    for (auto i : list) {
        free(i.first);
        free(i.second);
    }
}

/* Return the data in the specified list that corresponds
 *  to the specified key. */
char* mjpeg_getvalue(std::list<std::pair<char*, char*>>& list,
                     const char* key) {
    for (auto i : list) {
        // Check for matching key
        if (strcmp(i.first, key) == 0) {
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
    std::list<std::pair<char*, char*>> headerlist;

    int bytesread;

    /* Connect to the remote host. */
    sd = mjpeg_sck_connect(m_hostName.c_str(), m_port, m_cancelfdr);
    if (!mjpeg_sck_valid(sd)) {
        std::cerr << "mjpegrx: Connection failed\n";
        m_stopReceive = true;
        stopCallback();

        return;
    }
    m_sd = sd;

    // Send the HTTP request.
    snprintf(tmp, 255, "GET %s HTTP/1.0\r\n\r\n", m_requestPath.c_str());
    send(sd, tmp, strlen(tmp), 0);
    std::cout << tmp;

    while (!m_stopReceive) {
        // Read and parse incoming HTTP response headers.
        if (mjpeg_rxheaders(&headerbuf, &headerbufsize, sd,
                            m_cancelfdr) == -1) {
            std::cerr << "mjpegrx: recv(2) failed\n";
            break;
        }
        headerlist = mjpeg_process_header(headerbuf);
        free(headerbuf);
        if (headerlist.size() == 0) {
            break;
        }

        /* Read the Content-Length header to determine the length of data to
         * read.
         */
        asciisize = mjpeg_getvalue(
            headerlist,
            "Content-Length");

        if (asciisize == NULL) {
            mjpeg_freelist(headerlist);
            continue;
        }

        datasize = atoi(asciisize);
        mjpeg_freelist(headerlist);

        // Read the JPEG image data.
        buf = static_cast<char*>(malloc(datasize));
        bytesread = mjpeg_sck_recv(sd, buf, datasize, m_cancelfdr);
        if (bytesread != datasize) {
            free(buf);
            std::cerr << "mjpegrx: recv(2) failed\n";
            break;
        }

        // Load the image received (converts from JPEG to pixel array)
        uint8_t* ptr =
            jpeg_load_from_memory(reinterpret_cast<unsigned char*>(buf),
                                  datasize);

        if (ptr) {
            m_imageMutex.lock();

            // Free old buffer and store new one created by jpeg_load_from_memory()
            delete[] m_pxlBuf;

            m_pxlBuf = ptr;

            m_imageMutex.unlock();

            newImageCallback(buf, datasize);
        }
        else {
            std::cout << "MjpegClient: image failed to load\n";
        }

        free(buf);
    }

    // The loop has exited. We should now clean up and exit the thread.
    mjpeg_sck_close(sd);

    m_stopReceive = true;
    stopCallback();
}

