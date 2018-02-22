// Copyright (c) 2013-2018 FRC Team 3512. All Rights Reserved.

#include "VideoStream.hpp"

#include <cstring>
#include <iostream>

#include <QFont>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>

#include "../Util.hpp"
#include "ClientBase.hpp"

using namespace std::chrono_literals;

VideoStream::VideoStream(ClientBase* client, QWidget* parentWin, int width,
                         int height, WindowCallbacks* windowCallbacks,
                         std::function<void(void)> newImageCbk,
                         std::function<void(void)> startCbk,
                         std::function<void(void)> stopCbk)
    : QOpenGLWidget(parentWin),

      m_newImageCallback(newImageCbk),
      m_startCallback(startCbk),
      m_stopCallback(stopCbk) {
    connect(this, SIGNAL(redraw()), this, SLOT(repaint()));

    m_client = client;
    m_client->setObject(this);
    m_client->setNewImageCallback(&VideoStream::newImageCallback);
    m_client->setStartCallback(&VideoStream::startCallback);
    m_client->setStopCallback(&VideoStream::stopCallback);

    // Initialize the WindowCallbacks pointer
    m_windowCallbacks = windowCallbacks;

    setMinimumSize(width, height);

    m_imgWidth = width;
    m_imgHeight = height;

    m_updateThread = std::thread(&VideoStream::updateFunc, this);
}

VideoStream::~VideoStream() {
    m_client->stop();
    m_updateThread.join();
    delete m_client;
}

QSize VideoStream::sizeHint() const { return QSize(m_imgWidth, m_imgHeight); }

void VideoStream::setFPS(unsigned int fps) { m_frameRate = fps; }

void VideoStream::newImageCallback(uint8_t* buf, int bufsize) {
    (void)buf;
    (void)bufsize;

    // Send message to parent window about the new image
    if (std::chrono::system_clock::now() - m_displayTime >
        std::chrono::duration<double>(1.0 / m_frameRate)) {
        redraw();
        if (m_newImageCallback != nullptr) {
            m_newImageCallback();
        }

        {
            std::lock_guard<std::mutex> lock(m_imageMutex);
            m_img = m_client->getCurrentImage();
            m_imgWidth = m_client->getCurrentWidth();
            m_imgHeight = m_client->getCurrentHeight();
        }

        if (m_firstImage) {
            m_firstImage = false;
        }
    }

    m_imageAge = std::chrono::system_clock::now();
}

void VideoStream::startCallback() {
    if (m_client->isStreaming()) {
        m_firstImage = true;
        m_imageAge = std::chrono::system_clock::now();

        redraw();
        if (m_startCallback != nullptr) {
            m_startCallback();
        }
    }
}

void VideoStream::stopCallback() {
    redraw();
    if (m_stopCallback != nullptr) {
        m_stopCallback();
    }
}

void VideoStream::mousePressEvent(QMouseEvent* event) {
    if (m_windowCallbacks != nullptr) {
        m_windowCallbacks->clickEvent(event->x(), event->y());
    }
}

void VideoStream::paintGL() {
    QPainter painter(this);

    // If streaming is enabled
    if (m_client->isStreaming()) {
        // If no image has been received yet
        if (m_firstImage) {
            std::lock_guard<std::mutex> lock(m_imageMutex);
            painter.drawPixmap(0, 0, QPixmap::fromImage(m_connectImg));
        } else if (std::chrono::system_clock::now() - m_imageAge > 1s) {
            // If it's been too long since we received our last image

            // Display "Waiting..." over the last image received
            std::lock_guard<std::mutex> lock(m_imageMutex);
            painter.drawPixmap(0, 0, QPixmap::fromImage(m_waitImg));
        } else {
            // Else display the image last received
            std::lock_guard<std::mutex> lock(m_imageMutex);

            QImage tmp(m_img, m_imgWidth, m_imgHeight, QImage::Format_RGB888);
            QSize dstsize = tmp.size();
            dstsize.scale(size(), Qt::KeepAspectRatio);
            QSize offset = size() - dstsize;
            offset /= 2;
            painter.drawPixmap(offset.width(), offset.height(), dstsize.width(),
                               dstsize.height(), QPixmap::fromImage(tmp));
        }
    } else {
        // Else we aren't connected to the host; display disconnect graphic
        std::lock_guard<std::mutex> lock(m_imageMutex);
        painter.drawPixmap(0, 0, QPixmap::fromImage(m_disconnectImg));
    }

    m_displayTime = std::chrono::system_clock::now();
}

void VideoStream::resizeGL(int w, int h) {
    // Create the textures that can be displayed in the stream window
    recreateGraphics(w, h);
}

void VideoStream::recreateGraphics(int width, int height) {
    // Create intermediate buffers for graphics
    QImage connectBuf(width, height, QImage::Format_RGB888);
    QImage disconnectBuf(width, height, QImage::Format_RGB888);
    QImage waitBuf(width, height, QImage::Format_RGB888);

    QPainter p;

    /* ===== Fill graphics with a background color ===== */
    p.begin(&connectBuf);
    p.fillRect(0, 0, width, height, Qt::white);
    p.end();

    p.begin(&disconnectBuf);
    p.fillRect(0, 0, width, height, Qt::white);
    p.end();

    p.begin(&waitBuf);
    // Need a special background color since they will be transparent
    p.fillRect(0, 0, width, height, Qt::black);
    p.end();

    p.begin(&waitBuf);
    // Add transparent rectangle
    p.fillRect(width / 3, height / 3, width / 3, height / 3, Qt::darkGray);
    p.end();

    p.begin(&waitBuf);
    // Create background with transparency
    p.fillRect(0, 0, width, height, Qt::white);
    p.end();
    /* ================================================= */

    /* ===== Fill buffers with messages ===== */
    QFont font("Segoe UI", 14, QFont::Normal);
    font.setStyleHint(QFont::SansSerif);

    p.begin(&connectBuf);
    p.setFont(font);
    p.drawText(0, height / 2 - 16, width, 32, Qt::AlignCenter,
               tr("Connecting..."));
    p.end();

    p.begin(&disconnectBuf);
    p.setFont(font);
    p.drawText(0, height / 2 - 16, width, 32, Qt::AlignCenter,
               tr("Disconnected"));
    p.end();

    p.begin(&waitBuf);
    p.setFont(font);
    p.drawText(0, height / 2 - 16, width, 32, Qt::AlignCenter,
               tr("Waiting..."));
    p.end();
    /* ====================================== */

    std::lock_guard<std::mutex> lock(m_imageMutex);
    /* ===== Store bits from graphics in another buffer ===== */
    m_connectImg = connectBuf;
    m_disconnectImg = disconnectBuf;
    m_waitImg = waitBuf;
    /* ====================================================== */
}

void VideoStream::updateFunc() {
    std::chrono::duration<double> lastTime(0.0);
    std::chrono::duration<double> currentTime(0.0);

    while (m_client->isStreaming()) {
        currentTime = std::chrono::system_clock::now() - m_imageAge;

        // Make "Waiting..." graphic show up
        if (currentTime > 1s && lastTime <= 1s) {
            redraw();
        }

        lastTime = currentTime;

        std::this_thread::sleep_for(50ms);
    }
}
