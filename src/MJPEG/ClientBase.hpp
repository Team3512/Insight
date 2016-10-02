// Copyright (c) FRC Team 3512, Spartatroniks 2013-2016. All Rights Reserved.

#pragma once

#include <stdint.h>

#include <string>

class VideoStream;

/**
 * Base class for video stream providers
 */
class ClientBase {
public:
    virtual ~ClientBase() = default;

    // Request stream
    virtual void start() = 0;

    // Stop receiving stream
    virtual void stop() = 0;

    // Returns true if streaming is on
    virtual bool isStreaming() const = 0;

    // Saves most recently received image to a file
    virtual void saveCurrentImage(const std::string& fileName) = 0;

    /* Copies the most recently received image into a secondary internal buffer
     * and returns it to the user. After a call to this function, the new size
     * should be retrieved since it may have changed. Do NOT access the buffer
     * pointer returned while this function is executing.
     */
    virtual uint8_t* getCurrentImage() = 0;

    // Returns size of image currently in secondary buffer
    virtual unsigned int getCurrentWidth() const = 0;
    virtual unsigned int getCurrentHeight() const = 0;

    void setObject(VideoStream* object);
    void setNewImageCallback(void (VideoStream::*newImageCbk)(uint8_t* buf,
                                                              int bufsize));
    void setStartCallback(void (VideoStream::*startCbk)());
    void setStopCallback(void (VideoStream::*stopCbk)());

    void callNewImage(uint8_t* buf, int bufsize);
    void callStart();
    void callStop();

protected:
    VideoStream* m_object = nullptr;

    // Called if the new image loaded successfully
    void (VideoStream::*m_newImageCbk)(uint8_t* buf, int bufsize) = nullptr;

    // Called when client thread starts
    void (VideoStream::*m_startCbk)() = nullptr;

    // Called when client thread stops
    void (VideoStream::*m_stopCbk)() = nullptr;
};
