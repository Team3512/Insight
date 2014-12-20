//=============================================================================
//File Name: MjpegStream.hpp
//Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef MJPEG_STREAM_HPP
#define MJPEG_STREAM_HPP

/* This class creates a child window that receives MJPEG images and displays
 * them from a separate thread.
 *
 * To start using this class, just create an instance of it; everything else
 * is handled in a spawned thread
 *
 * Call startStream() to start the MJPEG stream or stopStream() to stop it
 * manually. This won't open or close the window.
 *
 * startStream() and stopStream() are called automatically in the constructor
 * and destructor respectively, but they can be called manually if desired.
 *
 * Change the button ID from IDC_STREAM_BUTTON to another ID if you want to
 * process more than one stream at once in WndProc
 *
 * Make sure every instance you create of this class is destroyed before its
 * respective parent window. If not, the application will crash.
 */

#include <QGLWidget>

class QMouseEvent;

#include <string>
#include <atomic>
#include <chrono>
#include <map>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>

#include "MjpegClient.hpp"

#include "WindowCallbacks.hpp"

class MjpegStream : public QGLWidget, public MjpegClient {
    Q_OBJECT

public:
    MjpegStream( const std::string& hostName ,
            unsigned short port ,
            const std::string& requestPath,
            QWidget* parentWin ,
            int width ,
            int height ,
            WindowCallbacks* windowCallbacks ,
            std::function<void(void)> newImageCallback = nullptr ,
            std::function<void(void)> startCallback = nullptr ,
            std::function<void(void)> stopCallback = nullptr
            );
    virtual ~MjpegStream();

    QSize sizeHint() const;

    // Set max frame rate of images displaying in window
    void setFPS( unsigned int fps );

protected:
    void newImageCallback( char* buf , int bufsize );
    void startCallback();
    void stopCallback();

    void mousePressEvent( QMouseEvent* event );
    void intializeGL();
    void paintGL();
    void resizeGL( int x , int y ); // Arguments are buffer dimensions

private:
    // Contains "Connecting" message
    uint8_t* m_connectPxl;

    // Contains "Disconnected" message
    uint8_t* m_disconnectPxl;

    // Contains "Waiting..." message
    uint8_t* m_waitPxl;

    // Stores image before displaying it on the screen
    uint8_t* m_img;
    unsigned int m_imgWidth;
    unsigned int m_imgHeight;
    unsigned int m_textureWidth;
    unsigned int m_textureHeight;
    std::mutex m_imageMutex;

    /* Set to true when a new image is received from the MJPEG server
     * Set back to false upon the first call to getCurrentImage()
     */
    std::atomic<bool> m_newImageAvailable;

    /* Used to determine when to draw the "Connecting..." message
     * (when the stream first starts)
     */
    std::atomic<bool> m_firstImage;

    // Determines when a video frame is old
    std::chrono::time_point<std::chrono::system_clock> m_imageAge;

    // Used to limit display frame rate
    std::chrono::time_point<std::chrono::system_clock> m_displayTime;
    unsigned int m_frameRate;

    // Locks window so only one thread can access or draw to it at a time
    std::mutex m_windowMutex;

    /* If false:
     *     Lets update thread run
     * If true:
     *     Closes update thread
     */
    std::atomic<bool> m_stopUpdate;

    WindowCallbacks* m_windowCallbacks;

    std::function<void(void)> m_newImageCallback;
    std::function<void(void)> m_startCallback;
    std::function<void(void)> m_stopCallback;

    // Makes sure "Waiting..." graphic is drawn after timeout
    std::thread* m_updateThread;

    /* Recreates the graphics that display messages in the stream window
     * (Resizes them and recenters the text in the window)
     */
    void recreateGraphics( int width , int height );

    // Function is used by m_updateThread
    void updateFunc();

signals:
    void redraw();
};

#endif // MJPEG_STREAM_HPP
