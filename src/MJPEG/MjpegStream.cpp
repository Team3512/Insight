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
#include <QPainter>
#include <QFont>

#include "stb_image.h"
#include "stb_image_write.h"

#include <iostream>
#include <cstring>

MjpegStream::MjpegStream( const std::string& hostName ,
        unsigned short port ,
        const std::string& requestPath ,
        QWidget* parentWin ,
        int width ,
        int height ,
        WindowCallbacks* windowCallbacks ,
        std::function<void(void)> newImageCbk ,
        std::function<void(void)> startCbk ,
        std::function<void(void)> stopCbk
        ) :
        QOpenGLWidget( parentWin ) ,
        MjpegClient( hostName , port , requestPath ) ,

        m_img( NULL ) ,
        m_imgWidth( 0 ) ,
        m_imgHeight( 0 ) ,
        m_textureWidth( 0 ) ,
        m_textureHeight( 0 ) ,

        m_firstImage( true ) ,

        m_frameRate( 15 ) ,

        m_newImageCallback( newImageCbk ) ,
        m_startCallback( startCbk ) ,
        m_stopCallback( stopCbk )
{
    connect( this , SIGNAL(redraw()) , this , SLOT(repaint()) );

    // Initialize the WindowCallbacks pointer
    m_windowCallbacks = windowCallbacks;

    setMinimumSize( width , height );

    m_imgWidth = width;
    m_imgHeight = height;

    m_updateThread = new std::thread( [this] { MjpegStream::updateFunc(); } );
}

MjpegStream::~MjpegStream() {
    stop();

    m_updateThread->join();
    delete m_updateThread;
}

QSize MjpegStream::sizeHint() const {
    return QSize( 320 , 240 );
}

void MjpegStream::setFPS( unsigned int fps ) {
    m_frameRate = fps;
}

void MjpegStream::newImageCallback( char* buf , int bufsize ) {
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

void MjpegStream::startCallback() {
    if ( isStreaming() ) {
        m_firstImage = true;
        m_imageAge = std::chrono::system_clock::now();

        redraw();
        if ( m_startCallback != nullptr ) {
            m_startCallback();
        }
    }
}

void MjpegStream::stopCallback() {
    redraw();
    if ( m_stopCallback != nullptr ) {
        m_stopCallback();
    }
}

void MjpegStream::mousePressEvent( QMouseEvent* event ) {
    m_windowCallbacks->clickEvent( event->x() , event->y() );
}

void MjpegStream::paintGL() {
    QPainter painter( this );

    // If streaming is enabled
    if ( isStreaming() ) {
        // If no image has been received yet
        if ( m_firstImage ) {
            m_imageMutex.lock();
            painter.drawPixmap( 0 , 0 , QPixmap::fromImage( m_connectImg ) );
            m_imageMutex.unlock();
        }

        // If it's been too long since we received our last image
        else if ( std::chrono::system_clock::now() - m_imageAge > std::chrono::milliseconds(1000) ) {
            // Display "Waiting..." over the last image received
            m_imageMutex.lock();
            painter.drawPixmap( 0 , 0 , QPixmap::fromImage( m_waitImg ) );
            m_imageMutex.unlock();
        }

        // Else display the image last received
        else {
            m_imageMutex.lock();

            QImage tmp( m_img , m_imgWidth , m_imgHeight , QImage::Format_RGBA8888 );
            painter.drawPixmap( 0 , 0 , QPixmap::fromImage( tmp ) );

            m_imageMutex.unlock();
        }
    }

    // Else we aren't connected to the host; display disconnect graphic
    else {
        m_imageMutex.lock();
        painter.drawPixmap( 0 , 0 , QPixmap::fromImage( m_disconnectImg ) );
        m_imageMutex.unlock();
    }

    m_displayTime = std::chrono::system_clock::now();
}

void MjpegStream::resizeGL( int w , int h ) {
    // Create the textures that can be displayed in the stream window
    recreateGraphics( w , h );
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
    /* ===== Store bits from graphics in another buffer ===== */
    m_connectImg = connectBuf;
    m_disconnectImg = disconnectBuf;
    m_waitImg = waitBuf;
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
