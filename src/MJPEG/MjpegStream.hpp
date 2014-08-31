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

#include <atomic>
#include <chrono>
#include <map>
#include <cstdint>

#include "../WinGDI/Text.hpp"
#include "../WinGDI/Vector.hpp"

#include "mjpegrx.h"
#include "mjpeg_thread.h"

#include "WindowCallbacks.hpp"

#define WM_MJPEGSTREAM_START     (WM_APP + 0x0001)
#define WM_MJPEGSTREAM_STOP      (WM_APP + 0x0002)
#define WM_MJPEGSTREAM_NEWIMAGE  (WM_APP + 0x0003)

class StreamClassInit;

class MjpegStream {
public:
    MjpegStream( const std::string& hostName ,
            unsigned short port ,
            const std::string& reqPath,
            HWND parentWin ,
            int xPosition ,
            int yPosition ,
            int width ,
            int height ,
            HINSTANCE appInstance,
            WindowCallbacks* windowCallbacks
            );
    virtual ~MjpegStream();

    Vector2i getPosition();
    void setPosition( const Vector2i& position );

    Vector2i getSize();
    void setSize( const Vector2i& size );

    // Request MJPEG stream and begin displaying it
    void startStream();

    // Stop receiving MJPEG stream
    void stopStream();

    // Returns true if streaming is on
    bool isStreaming();

    // Set max frame rate of images displaying in window
    void setFPS( unsigned int fps );

    // Displays the stream or a message if the stream isn't working
    void repaint();

    // Saves most recently received image to a file
    void saveCurrentImage( const std::string& fileName );

    /* Copies the most recently received image into a secondary internal buffer
     * and returns it to the user. After a call to this function, the new size
     * should be retrieved since it may have changed. Do NOT access the buffer
     * pointer returned while this function is executing.
     */
    uint8_t* getCurrentImage();

    // Returns size image currently in secondary buffer
    Vector2i getCurrentSize();

    /* Returns state of boolean 'm_newImageAvailable'. One could use this
     * instead of handling the WM_MJPEGSTREAM_NEWIMAGE message.
     */
    bool newImageAvailable();

protected:
    static void doneCallback( void* optarg );
    static void readCallback( char* buf , int bufsize , void* optarg );

private:
    std::string m_hostName;
    unsigned short m_port;
    std::string m_requestPath;

    HWND m_parentWin;

    HWND m_streamWin;

    // Resources for OpenGL rendering
    HGLRC m_threadRC;
    HDC m_bufferDC;

    // Holds pointer to button which toggles streaming
    HWND m_toggleButton;

    // Contains "Connecting" message
    Text m_connectMsg;
    BYTE* m_connectPxl;

    // Contains "Disconnected" message
    Text m_disconnectMsg;
    BYTE* m_disconnectPxl;

    // Contains "Waiting..." message
    Text m_waitMsg;
    BYTE* m_waitPxl;

    // Contains background color
    BYTE* m_backgroundPxl;

    // Stores image before displaying it on the screen
    uint8_t* m_pxlBuf;
    unsigned int m_imgWidth;
    unsigned int m_imgHeight;
    unsigned int m_textureWidth;
    unsigned int m_textureHeight;
    mjpeg_mutex_t m_imageMutex;

    /* Stores copy of image for use by external programs. It only updates when
     * getCurrentImage() is called.
     */
    uint8_t* m_extBuf;
    unsigned int m_extWidth;
    unsigned int m_extHeight;
    mjpeg_mutex_t m_extMutex;

    /* Set to true when a new image is received from the MJPEG server
     * Set back to false upon the first call to getCurrentImage()
     */
    std::atomic<bool> m_newImageAvailable;

    /* Used to determine when to draw the "Connecting..." message
     * (when the stream first starts)
     */
    std::atomic<bool> m_firstImage;

    // Used for streaming MJPEG frames from host
    struct mjpeg_callbacks_t m_callbacks;
    struct mjpeg_inst_t* m_streamInst;

    // Determines when a video frame is old
    std::chrono::time_point<std::chrono::system_clock> m_imageAge;

    // Used to limit display frame rate
    std::chrono::time_point<std::chrono::system_clock> m_displayTime;
    unsigned int m_frameRate;

    // Locks window so only one thread can access or draw to it at a time
    mjpeg_mutex_t m_windowMutex;

    /* If false:
     *     Lets receive thread run
     * If true:
     *     Closes receive thread
     */
    std::atomic<bool> m_stopReceive;

    /* If false:
     *     Lets update thread run
     * If true:
     *     Closes update thread
     */
    std::atomic<bool> m_stopUpdate;

    WindowCallbacks* m_windowCallbacks;

    /* Mouse position state variables */
    int m_lx;
    int m_ly;
    int m_cx;
    int m_cy;

    // Makes sure "Waiting..." graphic is drawn after timeout
    mjpeg_thread_t m_updateThread;

    /* Recreates the graphics that display messages in the stream window
     * (Resizes them and recenters the text in the window)
     */
    void recreateGraphics( const Vector2i& windowSize );

    // Initializes variables used for OpenGL rendering
    void EnableOpenGL();
    void DisableOpenGL();

    static void* updateFunc( void* obj );

    static std::map<HWND , MjpegStream*> m_map;
    static LRESULT CALLBACK WindowProc( HWND handle , UINT message , WPARAM wParam , LPARAM lParam );
    void paint( PAINTSTRUCT* ps );

    friend StreamClassInit;
};

#endif // MJPEG_STREAM_HPP
