//=============================================================================
//File Name: MjpegClient.hpp
//Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef MJPEG_CLIENT_HPP
#define MJPEG_CLIENT_HPP

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

#include <string>
#include <atomic>
#include <cstdint>

#include "../Vector.hpp"

#include "mjpegrx.h"
#include "mjpeg_thread.h"
#include "mjpeg_mutex.h"

class MjpegClient {
public:
    MjpegClient( const std::string& hostName , unsigned short port ,
            const std::string& requestPath );
    virtual ~MjpegClient();

    // Request MJPEG stream
    void start();

    // Stop receiving MJPEG stream
    void stop();

    // Returns true if streaming is on
    bool isStreaming() const;

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

    bool newImageAvailable() const;

protected:
    static void doneCallback( void* optarg );
    static void readCallback( char* buf , int bufsize , void* optarg );

    // Called at the end of doneCallback()
    virtual void done( void* optarg );

    // Called if the new image loaded successfully in readCallback()
    virtual void read( char* buf , int bufsize , void* optarg );

private:
    std::string m_hostName;
    unsigned short m_port;
    std::string m_requestPath;

    // Stores image before displaying it on the screen
    uint8_t* m_pxlBuf;
    unsigned int m_imgWidth;
    unsigned int m_imgHeight;
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

    // Used for streaming MJPEG frames from host
    struct mjpeg_callbacks_t m_callbacks;
    struct mjpeg_inst_t* m_streamInst;

    /* If false:
     *     Lets receive thread run
     * If true:
     *     Closes receive thread
     */
    std::atomic<bool> m_stopReceive;
};

#endif // MJPEG_CLIENT_HPP
