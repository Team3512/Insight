// Copyright (c) 2013-2018 FRC Team 3512. All Rights Reserved.

#include "MjpegClient.hpp"

#include <cstring>
#include <iostream>
#include <map>
#include <system_error>
#include <utility>

#include <QImage>

MjpegClient::MjpegClient(const std::string& hostName, unsigned short port,
                         const std::string& requestPath)
    : m_hostName(hostName), m_port(port), m_requestPath(requestPath) {
    mjpeg_socket_t pipefd[2];

    /* Create a pipe that, when written to, causes any operation in the
     * mjpegrx thread currently blocking to be cancelled.
     */
    if (mjpeg_pipe(pipefd) != 0) {
        throw std::system_error();
    }
    m_cancelfdr = pipefd[0];
    m_cancelfdw = pipefd[1];

    m_cinfo.err = jpeg_std_error(&m_jerr);
    m_cinfo.do_fancy_upsampling = 0;
    m_cinfo.do_block_smoothing = 0;

    jpeg_create_decompress(&m_cinfo);
}

MjpegClient::~MjpegClient() {
    stop();

    jpeg_destroy_decompress(&m_cinfo);

    mjpeg_sck_close(m_cancelfdr);
    mjpeg_sck_close(m_cancelfdw);
}

void MjpegClient::start() {
    if (!isStreaming()) {  // if stream is closed, reopen it
        // Join previous thread before making a new one
        if (m_recvThread.joinable()) {
            m_recvThread.join();
        }

        // Mark the thread as running
        m_stopReceive = false;

        m_recvThread = std::thread(&MjpegClient::recvFunc, this);
    }
}

void MjpegClient::stop() {
    if (isStreaming()) {
        m_stopReceive = true;

        // Cancel any currently blocking operations
        send(m_cancelfdw, "U", 1, 0);
    }

    // Close the receive thread
    if (m_recvThread.joinable()) {
        m_recvThread.join();
    }
}

bool MjpegClient::isStreaming() const { return !m_stopReceive; }

void MjpegClient::saveCurrentImage(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(m_imageMutex);

    QImage tmp(&m_pxlBuf[0], m_imgWidth, m_imgHeight, QImage::Format_RGB888);
    if (!tmp.save(fileName.c_str())) {
        std::cout << "MjpegClient: failed to save image to '" << fileName
                  << "'\n";
    }
}

uint8_t* MjpegClient::getCurrentImage() {
    std::lock_guard<std::mutex> imageLock(m_imageMutex);
    std::lock_guard<std::mutex> extLock(m_extMutex);

    // If buffer is wrong size, reallocate it
    if (m_imgWidth != m_extWidth || m_imgHeight != m_extHeight) {
        m_extWidth = m_imgWidth;
        m_extHeight = m_imgHeight;
    }

    m_extBuf = m_pxlBuf;

    return &m_extBuf[0];
}

unsigned int MjpegClient::getCurrentWidth() const {
    std::lock_guard<std::mutex> lock(m_extMutex);
    return m_extWidth;
}

unsigned int MjpegClient::getCurrentHeight() const {
    std::lock_guard<std::mutex> lock(m_extMutex);
    return m_extHeight;
}

bool MjpegClient::jpeg_load_from_memory(uint8_t* inputBuf, int inputLen,
                                        std::vector<uint8_t>& outputBuf) {
    // JPEG images start with bytes 0xFF, 0xD8 and end with bytes 0xFF, 0xD9.
    // Don't process data that isn't JPEG.
    if (inputBuf[0] != 0xFF || inputBuf[1] != 0xD8) {
        std::cout << "jpeg_load_from_memory: invalid magic: " << std::hex
                  << "0x" << static_cast<uint32_t>(inputBuf[0]) << ", "
                  << "0x" << static_cast<uint32_t>(inputBuf[1]) << std::dec
                  << std::endl;
        return false;
    }

    jpeg_mem_src(&m_cinfo, inputBuf, inputLen);
    if (jpeg_read_header(&m_cinfo, TRUE) != JPEG_HEADER_OK) {
        return false;
    }

    jpeg_start_decompress(&m_cinfo);

    int m_row_stride = m_cinfo.output_width * m_cinfo.output_components;

    m_imgWidth = m_cinfo.output_width;
    m_imgHeight = m_cinfo.output_height;
    m_imgChannels = m_cinfo.output_components;

    // Change size of output buffer if necessary
    if (outputBuf.size() != m_imgWidth * m_imgChannels * m_imgHeight) {
        outputBuf.resize(m_imgWidth * m_imgChannels * m_imgHeight);
    }

    uint8_t* sampleBuf;
    for (int outpos = 0; m_cinfo.output_scanline < m_cinfo.output_height;
         outpos += m_row_stride) {
        sampleBuf = &outputBuf[outpos];
        jpeg_read_scanlines(&m_cinfo, &sampleBuf, 1);
    }

    jpeg_finish_decompress(&m_cinfo);

    return true;
}

void MjpegClient::recvFunc() {
    ClientBase::callStart();

    std::vector<uint8_t> headerbuf;
    std::vector<uint8_t> buf;

    // Connect to the remote host.
    m_sd = mjpeg_sck_connect(m_hostName.c_str(), m_port, m_cancelfdr);
    if (!mjpeg_sck_valid(m_sd)) {
        std::cerr << "mjpegrx: Connection failed\n";
        m_stopReceive = true;
        ClientBase::callStop();

        return;
    }

    // Send the HTTP request.
    std::string tmp = "GET ";
    tmp += m_requestPath + " HTTP/1.0\r\n\r\n";
    send(m_sd, tmp.c_str(), tmp.length(), 0);
    std::cout << tmp;

    while (!m_stopReceive) {
        // Read and parse incoming HTTP response headers.
        if (mjpeg_rxheaders(headerbuf, m_sd, m_cancelfdr) == -1) {
            std::cerr << "mjpegrx: recv(2) failed\n";
            break;
        }
        std::string str(headerbuf.begin(), headerbuf.end());
        auto headerlist = mjpeg_process_header(std::move(str));
        if (headerlist.size() == 0) {
            break;
        }

        /* Read the Content-Length header to determine the length of data to
         * read.
         */
        std::string asciisize = headerlist["Content-Length"];

        if (asciisize == "") {
            continue;
        }

        int datasize = std::stoi(asciisize);

        // Read the JPEG image data.
        buf.resize(datasize);
        int bytesread = mjpeg_sck_recv(m_sd, &buf[0], datasize, m_cancelfdr);
        if (bytesread != datasize) {
            std::cerr << "mjpegrx: recv(2) failed\n";
            break;
        }

        // Load the image received (converts from JPEG to pixel array)
        bool decompressed = false;
        {
            std::lock_guard<std::mutex> lock(m_imageMutex);
            decompressed = jpeg_load_from_memory(&buf[0], datasize, m_pxlBuf);
        }

        if (decompressed) {
            ClientBase::callNewImage(&m_pxlBuf[0], m_pxlBuf.size());
        }
    }

    // The loop has exited. We should now clean up and exit the thread.
    mjpeg_sck_close(m_sd);

    m_stopReceive = true;

    ClientBase::callStop();
}

// Read data up until the character sequence "\r\n\r\n" is received.
int mjpeg_rxheaders(std::vector<uint8_t>& buf, int sd, int cancelfd) {
    size_t bufpos = 0;
    buf.resize(1024);

    while (!(bufpos >= 4 && buf[bufpos - 4] == '\r' &&
             buf[bufpos - 3] == '\n' && buf[bufpos - 2] == '\r' &&
             buf[bufpos - 1] == '\n')) {
        /* Read a byte from sd into &buf[bufpos]. If afterwards, bufpos ==
         * buf.size(), reallocate the buffer as 1024 bytes larger. This function
         * blocks until either a byte is received, or cancelfd becomes ready for
         * reading.
         */
        int bytesread = mjpeg_sck_recv(sd, &buf[bufpos], 1, cancelfd);
        if (bytesread != 1) {
            return -1;
        }
        bufpos++;

        if (bufpos == buf.size()) {
            buf.resize(buf.size() + 1024);
        }
    }

    return 0;
}

/* Processes the HTTP response headers, separating them into key-value pairs.
 * These are then stored in a map. The "header" argument should point to a block
 * of HTTP response headers in the standard ':' and '\n' separated format. The
 * key is the text on a line before the ':', the value is the text after the
 * ':', but before the '\n'. Any line without a ':' is ignored.
 */
std::map<std::string, std::string> mjpeg_process_header(std::string header) {
    std::map<std::string, std::string> list;

    if (header.length() == 0) {
        return list;
    }

    std::string key;
    std::string value;
    size_t startPos = 0;
    size_t endPos = 0;

    while (endPos != std::string::npos) {
        // Get the key
        endPos = header.find_first_of(":\n", startPos);
        key = header.substr(startPos, endPos - startPos);
        startPos = endPos + 1;

        // Get the value if a ':' exists on the line
        if (endPos != std::string::npos && header[endPos] == ':') {
            endPos = header.find('\r', startPos);
            value = header.substr(startPos, endPos - startPos);
            startPos = endPos + 1;

            list.emplace(key, value);
        }
    }

    return list;
}
