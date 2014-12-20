//=============================================================================
//File Name: MjpegStream.cpp
//Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "../Util.hpp"
#include "MjpegStream.hpp"

#include <QMouseEvent>
#include <QImage>
#include <QFont>

#include "stb_image.h"
#include "stb_image_write.h"

#include <GL/glu.h>

#include <iostream>
#include <cstring>

MjpegStream::MjpegStream( const std::string& hostName ,
        unsigned short port ,
        const std::string& requestPath ,
        QWidget* parentWin ,
        int width ,
        int height ,
        WindowCallbacks* windowCallbacks ,
        std::function<void(void)> newImageCallback ,
        std::function<void(void)> startCallback ,
        std::function<void(void)> stopCallback
        ) :
        QGLWidget( parentWin ) ,
        MjpegClient( hostName , port , requestPath ) ,

        m_connectPxl( NULL ) ,
        m_disconnectPxl( NULL ) ,
        m_waitPxl( NULL ) ,

        m_img( NULL ) ,
        m_imgWidth( 0 ) ,
        m_imgHeight( 0 ) ,
        m_textureWidth( 0 ) ,
        m_textureHeight( 0 ) ,

        m_firstImage( true ) ,

        m_frameRate( 15 ) ,

        m_stopUpdate( true ) ,

        m_newImageCallback( newImageCallback ) ,
        m_startCallback( startCallback ) ,
        m_stopCallback( stopCallback )
{
    connect( this , SIGNAL(redraw()) , this , SLOT(updateGL()) );

    // Initialize the WindowCallbacks pointer
    m_windowCallbacks = windowCallbacks;

    setMinimumSize( width , height );

    m_imgWidth = width;
    m_imgHeight = height;

    m_stopUpdate = false;
    m_updateThread = new std::thread( [this] { MjpegStream::updateFunc(); } );
}

MjpegStream::~MjpegStream() {
    stop();

    m_stopUpdate = true;
    m_updateThread->join();
    delete m_updateThread;

    delete[] m_connectPxl;
    delete[] m_disconnectPxl;
    delete[] m_waitPxl;
}

QSize MjpegStream::sizeHint() const {
    return QSize( 320 , 240 );
}

void MjpegStream::start() {
    if ( !isStreaming() ) {
        MjpegClient::start();
        if ( isStreaming() ) {
            m_firstImage = true;
            m_imageAge = std::chrono::system_clock::now();

            redraw();
            if ( m_startCallback != nullptr ) {
                m_startCallback();
            }
        }
    }
}

void MjpegStream::stop() {
    // If stream is open, close it
    if ( isStreaming() ) {
        MjpegClient::stop();
        redraw();
        if ( m_stopCallback != nullptr ) {
            m_stopCallback();
        }
    }
}

void MjpegStream::setFPS( unsigned int fps ) {
    m_frameRate = fps;
}

void MjpegStream::done() {
    redraw();
}

void MjpegStream::read( char* buf , int bufsize ) {
    // Send message to parent window about the new image
    if ( std::chrono::system_clock::now() - m_displayTime > std::chrono::duration<double>(1.0 / m_frameRate) ) {
        redraw();
        if ( m_newImageCallback != nullptr ) {
            m_newImageCallback();
        }

        m_imageMutex.lock();

        m_img = getCurrentImage();

        m_imgWidth = getCurrentWidth();
        m_imgHeight = getCurrentHeight();

        m_imageMutex.unlock();

        if ( m_firstImage ) {
            m_firstImage = false;
        }
    }

    m_imageAge = std::chrono::system_clock::now();
}

void MjpegStream::mousePressEvent( QMouseEvent* event ) {
    m_windowCallbacks->clickEvent( event->x() , event->y() );
}

void MjpegStream::intializeGL() {
    glClearColor( 1.f , 0.f , 1.f , 1.f );
    glClearDepth( 1.f );
    glClear( GL_COLOR_BUFFER_BIT );

    glDepthFunc( GL_LESS );
    glDepthMask( GL_FALSE );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_BLEND );
    glDisable( GL_ALPHA_TEST );
    glEnable( GL_TEXTURE_2D );
    glBlendFunc( GL_SRC_ALPHA , GL_ONE_MINUS_SRC_ALPHA );
    glShadeModel( GL_FLAT );

    glEnable( GL_TEXTURE_2D );
    glTexParameteri( GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , GL_LINEAR );

    // Check for OpenGL errors
    int glError = glGetError();
    if ( glError != GL_NO_ERROR ) {
        std::cerr << __FILE__ << " OpenGL failure: " << gluErrorString( glError ) << "\n";
    }
}

void MjpegStream::paintGL() {
    std::cout << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "] "
              << "paint\n";
    int textureSize;

    /* If our image won't fit in the texture, make a bigger one whose width and
     * height are a power of two.
     */
    if( m_imgWidth > m_textureWidth || m_imgHeight > m_textureHeight ) {
        textureSize = npot( std::max( m_imgWidth , m_imgHeight ) );

        uint8_t* tmpBuf = new uint8_t[textureSize * textureSize * 3];
        glTexImage2D( GL_TEXTURE_2D , 0 , 3 , textureSize , textureSize , 0 ,
                GL_RGB , GL_UNSIGNED_BYTE , tmpBuf );
        delete[] tmpBuf;

        m_textureWidth = textureSize;
        m_textureHeight = textureSize;
    }

    /* Represents the amount of the texture to display. These are ratios
     * between the dimensions of the image in the texture and the actual
     * texture dimensions. Once these are set for the specific image to be
     * displayed, they are passed into glTexCoord2f.
     */
    double wratio = 0.f;
    double hratio = 0.f;

    // If streaming is enabled
    if ( isStreaming() ) {
        // If no image has been received yet
        if ( m_firstImage ) {
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , width() ,
                    height() , GL_RGBA , GL_UNSIGNED_BYTE , m_connectPxl );

            m_imageMutex.unlock();

            wratio = (float)width() / (float)m_textureWidth;
            hratio = (float)height() / (float)m_textureHeight;
        }

        // If it's been too long since we received our last image
        else if ( std::chrono::system_clock::now() - m_imageAge > std::chrono::milliseconds(1000) ) {
            // Display "Waiting..." over the last image received
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , width() ,
                    height() , GL_RGBA , GL_UNSIGNED_BYTE , m_waitPxl );

            m_imageMutex.unlock();

            wratio = (float)width() / (float)m_textureWidth;
            hratio = (float)height() / (float)m_textureHeight;
        }

        // Else display the image last received
        else {
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , m_imgWidth ,
                    m_imgHeight, GL_RGBA , GL_UNSIGNED_BYTE , m_img );

            m_imageMutex.unlock();

            wratio = (float)m_imgWidth / (float)m_textureWidth;
            hratio = (float)m_imgHeight / (float)m_textureHeight;
        }
    }

    // Else we aren't connected to the host; display disconnect graphic
    else {
        m_imageMutex.lock();

        glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , width() , height() ,
                GL_RGBA , GL_UNSIGNED_BYTE , m_disconnectPxl );

        m_imageMutex.unlock();

        wratio = (float)width() / (float)m_textureWidth;
        hratio = (float)height() / (float)m_textureHeight;
    }

    // Position the GL texture in the Win32 window
    glBegin( GL_TRIANGLE_FAN );
    glColor4f( 1.f , 1.f , 1.f , 1.f );
    glTexCoord2f( 0 , 0 ); glVertex2f( 0 , 0 );
    glTexCoord2f( wratio , 0 ); glVertex2f( width() , 0 );
    glTexCoord2f( wratio , hratio ); glVertex2f( width() , height() );
    glTexCoord2f( 0 , hratio ); glVertex2f( 0 , height() );
    glEnd();

    // Check for OpenGL errors
    GLenum glError = glGetError();
    if( glError != GL_NO_ERROR ) {
        std::cerr << "Failed to draw texture: " << gluErrorString( glError ) << "\n";
    }

    m_displayTime = std::chrono::system_clock::now();
}

void MjpegStream::resizeGL( int x , int y ) {
    // Set up screen
    glViewport( 0 , 0 , x , y );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0 , x , y , 0 , -1.f , 1.f );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    // Check for OpenGL errors
    GLenum glError = glGetError();
    if ( glError != GL_NO_ERROR ) {
        std::cerr << __FILE__ << " OpenGL failure: " << gluErrorString( glError ) << "\n";
    }

    // Create the textures that can be displayed in the stream window
    recreateGraphics( x , y );
}

void MjpegStream::recreateGraphics( int width , int height ) {
    // Create intermediate buffers for graphics
    QImage connectBuf( width , height , QImage::Format_RGBA8888 );
    QImage disconnectBuf( width , height , QImage::Format_RGBA8888 );
    QImage waitBuf( width , height , QImage::Format_RGBA8888 );

    QPainter p;

    /* ===== Fill graphics with a background color ===== */
    p.begin( &connectBuf );
    p.fillRect( 0 , 0 , width , height , Qt::white );
    p.end();

    p.begin( &disconnectBuf );
    p.fillRect( 0 , 0 , width , height , Qt::white );
    p.end();

    p.begin( &waitBuf );
    // Need a special background color since they will be transparent
    p.fillRect( 0 , 0 , width , height , Qt::black );
    p.end();

    p.begin( &waitBuf );
    // Add transparent rectangle
    p.fillRect( width / 3 , height / 3 , width / 3 ,
            height / 3 , Qt::darkGray );
    p.end();

    p.begin( &waitBuf );
    // Create background with transparency
    p.fillRect( 0 , 0 , width , height , Qt::white );
    p.end();
    /* ================================================= */

    /* ===== Fill buffers with messages ===== */
    QFont font( "Segoe UI" , 14 , QFont::Normal );
    font.setStyleHint( QFont::SansSerif );

    p.begin( &connectBuf );
    p.setFont( font );
    p.drawText( 0 , height / 2 - 16 , width , 32 , Qt::AlignCenter ,
            tr("Connecting...") );
    p.end();

    p.begin( &disconnectBuf );
    p.setFont( font );
    p.drawText( 0 , height / 2 - 16 , width , 32 , Qt::AlignCenter ,
            tr("Disconnected") );
    p.end();

    p.begin( &waitBuf );
    p.setFont( font );
    p.drawText( 0 , height / 2 - 16 , width , 32 , Qt::AlignCenter ,
            tr("Waiting...") );
    p.end();
    /* ====================================== */

    m_imageMutex.lock();
    /* ===== Allocate buffers for pixels of graphics ===== */
    delete[] m_connectPxl;
    m_connectPxl = new uint8_t[width * height * 4];

    delete[] m_disconnectPxl;
    m_disconnectPxl = new uint8_t[width * height * 4];

    delete[] m_waitPxl;
    m_waitPxl = new uint8_t[width * height * 4];
    /* =================================================== */

    /* ===== Store bits from graphics in another buffer ===== */
    connectBuf.save( "connectBuf.png" );
    disconnectBuf.save( "disconnectBuf.png" );
    waitBuf.save( "waitBuf.png" );

    std::memcpy( m_connectPxl , connectBuf.bits() , connectBuf.byteCount() );
    std::memcpy( m_disconnectPxl , disconnectBuf.bits() , disconnectBuf.byteCount() );
    std::memcpy( m_waitPxl , waitBuf.bits() , waitBuf.byteCount() );
    /* ====================================================== */
    m_imageMutex.unlock();
}

void MjpegStream::updateFunc() {
    std::chrono::duration<double> lastTime( 0.0 );
    std::chrono::duration<double> currentTime( 0.0 );

    while ( isStreaming() ) {
        currentTime = std::chrono::system_clock::now() - m_imageAge;

        // Make "Waiting..." graphic show up
        if ( currentTime > std::chrono::milliseconds(1000) && lastTime <= std::chrono::milliseconds(1000) ) {
            redraw();
        }

        lastTime = currentTime;

        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    }
}
