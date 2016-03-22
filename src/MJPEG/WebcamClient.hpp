// =============================================================================
// Description: Receives a video stream from a webcam and displays it in a child
//              window with the specified properties
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#ifndef WEBCAM_CLIENT_HPP
#define WEBCAM_CLIENT_HPP

#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/videoio.hpp>

#include "ClientBase.hpp"

class WebcamClient : public ClientBase {
public:
    explicit WebcamClient(int device = 0);
    virtual ~WebcamClient();

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
    cv::VideoCapture m_cap{0};
    int m_device;

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

    // Used by m_recvThread
    void recvFunc();
};

#endif // WEBCAM_CLIENT_HPP
