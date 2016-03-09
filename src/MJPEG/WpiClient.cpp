// =============================================================================
// Description: Receives a video stream from WPILib's CameraServer class and
//              displays it in a child window with the specified properties
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include "WpiClient.hpp"
#include "mjpeg_sck_selector.hpp"
#include "MjpegClient.hpp"

#include <QImage>

#include <iostream>
#include <system_error>
#include <map>
#include <cstring>

constexpr uint8_t WpiClient::k_magicNumber[];

WpiClient::WpiClient(const std::string& hostName) : m_hostName(hostName) {
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

WpiClient::~WpiClient() {
    stop();

    jpeg_destroy_decompress(&m_cinfo);

    mjpeg_sck_close(m_cancelfdr);
    mjpeg_sck_close(m_cancelfdw);
}

void WpiClient::start() {
    if (!isStreaming()) { // if stream is closed, reopen it
        // Join previous thread before making a new one
        if (m_recvThread.joinable()) {
            m_recvThread.join();
        }

        // Mark the thread as running
        m_stopReceive = false;

        m_recvThread = std::thread(&WpiClient::recvFunc, this);
    }
}

void WpiClient::stop() {
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

bool WpiClient::isStreaming() const {
    return !m_stopReceive;
}

void WpiClient::saveCurrentImage(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(m_imageMutex);

    QImage tmp(&m_pxlBuf[0], m_imgWidth, m_imgHeight, QImage::Format_RGB888);
    if (!tmp.save(fileName.c_str())) {
        std::cout << "WpiClient: failed to save image to '" << fileName <<
            "'\n";
    }
}

uint8_t* WpiClient::getCurrentImage() {
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

unsigned int WpiClient::getCurrentWidth() const {
    std::lock_guard<std::mutex> lock(m_extMutex);
    return m_extWidth;
}

unsigned int WpiClient::getCurrentHeight() const {
    std::lock_guard<std::mutex> lock(m_extMutex);
    return m_extHeight;
}

void WpiClient::jpeg_load_from_memory(uint8_t* inputBuf,
                                      int inputLen,
                                      std::vector<uint8_t>& outputBuf) {
    jpeg_mem_src(&m_cinfo, inputBuf, inputLen);
    if (jpeg_read_header(&m_cinfo, TRUE) != JPEG_HEADER_OK) {
        return;
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
}

void WpiClient::recvFunc() {
    ClientBase::callStart();

    std::vector<uint8_t> buf;

    // Connect to the remote host.
    m_sd = mjpeg_sck_connect(m_hostName.c_str(), k_port, m_cancelfdr);
    if (!mjpeg_sck_valid(m_sd)) {
        std::cerr << "mjpegrx: Connection failed\n";
        m_stopReceive = true;
        ClientBase::callStop();

        return;
    }

    int bytesRead = 0;
    int dataSize = 0;

    // Send request
    Request request;
    request.fps = htonl(15);
    request.compression = htonl(k_hardwareCompression);
    request.size = htonl(k_size320x240);

    send(m_sd, &request, sizeof(Request), 0);

    while (!m_stopReceive) {
        // Read magic numbers
        bytesRead = mjpeg_sck_recv(m_sd, magic, sizeof(magic), m_cancelfdr);

        if (bytesRead != sizeof(magic) ||
            std::strncmp(reinterpret_cast<const char*>(magic),
                         reinterpret_cast<const char*>(k_magicNumber),
                         sizeof(magic)) != 0) {
            std::cerr << "recv(2) failed\n";
            break;
        }

        // Read image size
        bytesRead = mjpeg_sck_recv(m_sd, &dataSize, sizeof(dataSize),
                                   m_cancelfdr);

        if (bytesRead != sizeof(dataSize)) {
            std::cerr << "recv(2) failed\n";
            break;
        }

        dataSize = ntohl(dataSize);

        // Read the JPEG image data
        buf.resize(dataSize);
        bytesRead = mjpeg_sck_recv(m_sd, &buf[0], dataSize, m_cancelfdr);
        if (bytesRead != dataSize) {
            std::cerr << "recv(2) failed\n";
            break;
        }

        // Load the image received (converts from JPEG to pixel array)
        {
            std::lock_guard<std::mutex> lock(m_imageMutex);
            jpeg_load_from_memory(&buf[0], dataSize, m_pxlBuf);
        }

        ClientBase::callNewImage(&m_pxlBuf[0], m_pxlBuf.size());
    }

    // The loop has exited. We should now clean up and exit the thread.
    mjpeg_sck_close(m_sd);

    m_stopReceive = true;

    ClientBase::callStop();
}
