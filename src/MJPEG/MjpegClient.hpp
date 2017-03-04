// Copyright (c) FRC Team 3512, Spartatroniks 2013-2017. All Rights Reserved.

#pragma once

#include <stdint.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <jpeglib.h>

#include "ClientBase.hpp"
#include "mjpeg_sck.hpp"

/**
 * Receives an MJPEG stream and displays it in a child window with the specified
 * properties
 */
class MjpegClient : public ClientBase {
public:
    MjpegClient(const std::string& hostName, unsigned short port,
                const std::string& requestPath);
    virtual ~MjpegClient();

    // Request MJPEG stream
    void start();

    // Stop receiving MJPEG stream
    void stop();

    // Returns true if streaming is on
    bool isStreaming() const;

    // Saves most recently received image to a file
    void saveCurrentImage(const std::string& fileName);

    /* Copies the most recently received image into a secondary internal buffer
     * and returns it to the user. After a call to this function, the new size
     * should be retrieved since it may have changed. Do NOT access the buffer
     * pointer returned while this function is executing.
     */
    uint8_t* getCurrentImage();

    // Returns size of image currently in secondary buffer
    unsigned int getCurrentWidth() const;
    unsigned int getCurrentHeight() const;

private:
    std::string m_hostName;
    uint16_t m_port;
    std::string m_requestPath;

    // Stores image before displaying it on the screen
    std::vector<uint8_t> m_pxlBuf;
    unsigned int m_imgWidth = 0;
    unsigned int m_imgHeight = 0;
    unsigned int m_imgChannels = 0;
    mutable std::mutex m_imageMutex;

    /* Stores copy of image for use by external programs. It only updates when
     * getCurrentImage() is called.
     */
    std::vector<uint8_t> m_extBuf;
    unsigned int m_extWidth = 0;
    unsigned int m_extHeight = 0;
    mutable std::mutex m_extMutex;

    std::thread m_recvThread;

    /* If false:
     *     Lets receive thread run
     * If true:
     *     Closes receive thread
     */
    std::atomic<bool> m_stopReceive{true};

    mjpeg_socket_t m_cancelfdr = 0;
    mjpeg_socket_t m_cancelfdw = 0;
    mjpeg_socket_t m_sd = INVALID_SOCKET;

    struct jpeg_decompress_struct m_cinfo;
    struct jpeg_error_mgr m_jerr;
    JSAMPARRAY m_buffer = nullptr; /* Output row buffer */

    // Used by m_recvThread
    void recvFunc();

    /* inputBuf is input JPEG data; width, height, and channel amount are stored
     * in member variables
     */
    void jpeg_load_from_memory(uint8_t* inputBuf, int inputLen,
                               std::vector<uint8_t>& outputBuf);
};

int mjpeg_rxheaders(std::vector<uint8_t>& buf, int sd, int cancelfd);

std::map<std::string, std::string> mjpeg_process_header(std::string header);
