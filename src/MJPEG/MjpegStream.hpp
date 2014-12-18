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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <string>
#include <atomic>
#include <chrono>
#include <map>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>

#include "MjpegClient.hpp"
#include "../WinGDI/Text.hpp"
#include "../Vector.hpp"
#include "../GLWindow.hpp"

#include "WindowCallbacks.hpp"

void BMPtoPXL( HDC dc , HBITMAP bmp , uint8_t* pxlData );

class StreamClassInit;

class MjpegStream : public MjpegClient {
public:
    MjpegStream( const std::string& hostName ,
            unsigned short port ,
            const std::string& requestPath,
            HWND parentWin ,
            int xPosition ,
            int yPosition ,
            int width ,
            int height ,
            HINSTANCE appInstance,
            WindowCallbacks* windowCallbacks ,
            std::function<void(void)> newImageCallback = nullptr ,
            std::function<void(void)> startCallback = nullptr ,
            std::function<void(void)> stopCallback = nullptr
            );
    virtual ~MjpegStream();

    Vector2i getPosition();
    void setPosition( const Vector2i& position );

    Vector2i getSize();
    void setSize( const Vector2i& size );

    // Request MJPEG stream and begin displaying it
    void start();

    // Stop receiving MJPEG stream
    void stop();

    // Set max frame rate of images displaying in window
    void setFPS( unsigned int fps );

    // Displays the stream or a message if the stream isn't working
    void repaint();

protected:
    void done();
    void read( char* buf , int bufsize );

private:
    HWND m_parentWin;

    HWND m_streamWin;
    GLWindow* m_glWin;

    // Holds pointer to button which toggles streaming
    HWND m_toggleButton;

    // Contains "Connecting" message
    Text m_connectMsg;
    uint8_t* m_connectPxl;

    // Contains "Disconnected" message
    Text m_disconnectMsg;
    uint8_t* m_disconnectPxl;

    // Contains "Waiting..." message
    Text m_waitMsg;
    uint8_t* m_waitPxl;

    // Contains background color
    uint8_t* m_backgroundPxl;

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

    /* Mouse position state variables */
    int m_lx;
    int m_ly;
    int m_cx;
    int m_cy;

    // Makes sure "Waiting..." graphic is drawn after timeout
    std::thread* m_updateThread;

    /* Recreates the graphics that display messages in the stream window
     * (Resizes them and recenters the text in the window)
     */
    void recreateGraphics( const Vector2i& windowSize );

    // Function is used by m_updateThread
    void updateFunc();

    static std::map<HWND , MjpegStream*> m_map;
    static LRESULT CALLBACK WindowProc( HWND handle , UINT message , WPARAM wParam , LPARAM lParam );
    void paint( PAINTSTRUCT* ps );

    friend StreamClassInit;
};

#endif // MJPEG_STREAM_HPP
