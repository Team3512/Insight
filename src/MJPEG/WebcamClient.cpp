// =============================================================================
// File Name: WebcamClient.cpp
// Description: Receives a video stream from a webcam and displays it in a child
//             window with the specified properties
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include <QImage>
#include <QString>

#include <iostream>
#include <opencv2/imgproc.hpp>

#include "WebcamClient.hpp"

WebcamClient::WebcamClient(int device) : m_cap(device), m_device(device) {
}

WebcamClient::~WebcamClient() {
    stop();
}

void WebcamClient::start() {
    if (!isStreaming()) { // if stream is closed, reopen it
        // Join previous thread before making a new one
        if (m_recvThread.joinable()) {
            m_recvThread.join();
        }

        // Mark the thread as running
        m_stopReceive = false;

        m_recvThread = std::thread(&WebcamClient::recvFunc, this);
    }
}

void WebcamClient::stop() {
    if (isStreaming()) {
        m_stopReceive = true;
    }

    // Close the receive thread
    if (m_recvThread.joinable()) {
        m_recvThread.join();
    }
}

bool WebcamClient::isStreaming() const {
    return !m_stopReceive;
}

void WebcamClient::saveCurrentImage(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(m_imageMutex);

    QImage tmp(&m_pxlBuf[0], m_imgWidth, m_imgHeight, QImage::Format_RGB888);
    if (!tmp.save(fileName.c_str())) {
        std::cout << "WebcamClient: failed to save image to '" << fileName <<
            "'\n";
    }
}

uint8_t* WebcamClient::getCurrentImage() {
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

unsigned int WebcamClient::getCurrentWidth() const {
    std::lock_guard<std::mutex> lock(m_extMutex);
    return m_extWidth;
}

unsigned int WebcamClient::getCurrentHeight() const {
    std::lock_guard<std::mutex> lock(m_extMutex);
    return m_extHeight;
}

void WebcamClient::recvFunc() {
    ClientBase::callStart();

    std::vector<uint8_t> buf;

    // Connect to the remote host.
    m_cap.open(m_device);
    if (!m_cap.isOpened()) {
        std::cerr << "mjpegrx: Connection failed\n";
        m_stopReceive = true;
        ClientBase::callStop();

        return;
    }

    while (!m_stopReceive) {
        cv::Mat frame;
        m_cap >> frame;
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        m_imgWidth = frame.cols;
        m_imgHeight = frame.rows;
        m_imgChannels = frame.channels();

        buf.resize(m_imgHeight * m_imgWidth * m_imgChannels);
        buf.assign(frame.datastart, frame.dataend);

        // Copy image to user-accessible buffer
        {
            std::lock_guard<std::mutex> lock(m_imageMutex);
            m_pxlBuf = buf;
        }

        ClientBase::callNewImage(&m_pxlBuf[0], m_pxlBuf.size());
    }

    ClientBase::callStop();
}

