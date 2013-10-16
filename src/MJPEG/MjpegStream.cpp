//=============================================================================
//File Name: MjpegStream.cpp
//Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "../WinGDI/UIFont.hpp"
#include "MjpegStream.hpp"
#include "../Resource.h"
#include "ImageScale.hpp"

#include <iostream>
#include <sstream>
#include <wingdi.h>
#include <cstring>

std::map<HWND , MjpegStream*> MjpegStream::m_map;

// Bit-twiddling hack: Return the next power of two
int npot( int num ) {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num++;

    return num;
}

void BMPtoPXL( HDC dc , HBITMAP bmp , int width , int height , BYTE* pxlData ) {
    BITMAPINFOHEADER bmi = {0};
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biWidth = width;
    bmi.biHeight = -height;
    bmi.biCompression = BI_RGB;
    bmi.biSizeImage = 0;// 3 * ScreenX * ScreenY; for PNG or JPEG

    GetDIBits( dc , bmp , 0 , height , pxlData , (BITMAPINFO*)&bmi , DIB_RGB_COLORS );
}

class StreamClassInit {
public:
    StreamClassInit();
    ~StreamClassInit();

private:
    WNDCLASSEX m_windowClass;
};

StreamClassInit windowClass;

StreamClassInit::StreamClassInit() {
    m_windowClass.cbSize        = sizeof(WNDCLASSEX);
    m_windowClass.style         = CS_OWNDC;
    m_windowClass.lpfnWndProc   = &MjpegStream::WindowProc;
    m_windowClass.cbClsExtra    = 0;
    m_windowClass.cbWndExtra    = 0;
    m_windowClass.hInstance     = GetModuleHandle( NULL );
    m_windowClass.hIcon         = NULL;
    m_windowClass.hCursor       = LoadCursor( NULL , IDC_ARROW );
    m_windowClass.hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
    m_windowClass.lpszMenuName  = NULL;
    m_windowClass.lpszClassName = "Stream";
    m_windowClass.hIconSm       = NULL;

    RegisterClassEx( &m_windowClass );
}

StreamClassInit::~StreamClassInit() {
    UnregisterClass( "Stream" , GetModuleHandle( NULL ) );
}

MjpegStream::MjpegStream( const std::string& hostName ,
        unsigned short port ,
        const std::string& requestPath ,
        HWND parentWin ,
        int xPosition ,
        int yPosition ,
        int width ,
        int height ,
        HINSTANCE appInstance
        ) :
        m_hostName( hostName ) ,
        m_port( port ) ,
        m_requestPath( requestPath ) ,

        m_threadRC( NULL ) ,
        m_bufferDC( NULL ) ,

        m_connectMsg( Vector2i( 0 , 0 ) , UIFont::getInstance().segoeUI18() ,
                L"Connecting..." , RGB( 0 , 0 , 0 ) , RGB( 255 , 255 , 255 ) ,
                false ) ,
        m_connectPxl( NULL ) ,
        m_disconnectMsg( Vector2i( 0 , 0 ) , UIFont::getInstance().segoeUI18() ,
                L"Disconnected" , RGB( 0 , 0 , 0 ) , RGB( 255 , 255 , 255 ) ,
                false ) ,
        m_disconnectPxl( NULL ) ,
        m_waitMsg( Vector2i( 0 , 0 ) , UIFont::getInstance().segoeUI18() ,
                L"Waiting..." , RGB( 0 , 0 , 0 ) , RGB( 255 , 255 , 255 ) ,
                false ) ,
        m_waitPxl( NULL ) ,
        m_backgroundPxl( NULL ) ,

        m_pxlBuf( NULL ) ,
        m_imgWidth( 0 ) ,
        m_imgHeight( 0 ) ,
        m_dispWidth( 0 ) ,
        m_dispHeight( 0 ) ,

        m_firstImage( true ) ,

        m_streamInst( NULL ) ,

        m_stopReceive( true ) ,

        m_updateThread( [&]{
                int lastTime = 0;
                int currentTime = 0;

                while ( !m_stopReceive ) {
                    currentTime = m_imageAge.getElapsedTime().asMilliseconds();

                    if ( currentTime > 1000 && lastTime <= 1000 ) {
                        PostMessage( m_parentWin , WM_MJPEGSTREAM_NEWIMAGE , 0 , 0 );
                    }

                    lastTime = currentTime;

                    Sleep( 100 );
                }
} ) {
    m_parentWin = parentWin;

    m_streamWin = CreateWindowEx( 0 ,
        "Stream" ,
        "" ,
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE ,
        xPosition ,
        yPosition ,
        width ,
        height ,
        parentWin ,
        NULL ,
        appInstance ,
        NULL );

    /* ===== Initialize the stream toggle button ===== */
    HGDIOBJ hfDefault = GetStockObject( DEFAULT_GUI_FONT );

    m_toggleButton = CreateWindowEx( 0,
        "BUTTON",
        "Start Stream",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        xPosition ,
        yPosition + height + 5,
        100,
        28,
        m_parentWin,
        reinterpret_cast<HMENU>( IDC_STREAM_BUTTON ),
        GetModuleHandle( NULL ),
        NULL);

    SendMessage(m_toggleButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>( hfDefault ),
        MAKELPARAM( FALSE , 0 ) );
    /* =============================================== */

    // Create the textures that can be displayed in the stream window
    recreateGraphics( Vector2i( width , height ) );

    // Set up the callback description structure
    ZeroMemory( &m_callbacks , sizeof(struct mjpeg_callbacks_t) );
    m_callbacks.readcallback = readCallback;
    m_callbacks.donecallback = doneCallback;
    m_callbacks.optarg = this;

    // Add window to global map
    m_map.insert( m_map.begin() ,
            std::pair<HWND , MjpegStream*>( m_streamWin , this ) );

    /* This isn't called in response to the WM_CREATE message because an entry
     * in m_map for the window must exist first, but WM_CREATE is sent to the
     * message queue immediately upon creation of the window, which is before
     * this constructor has a chance to add the entry.
     */
    EnableOpenGL();

    m_updateThread.launch();
}

MjpegStream::~MjpegStream() {
    stopStream();

    delete[] m_connectPxl;
    delete[] m_disconnectPxl;
    delete[] m_waitPxl;
    delete[] m_backgroundPxl;

    DestroyWindow( m_streamWin );
    DestroyWindow( m_toggleButton );

    // Remove window from global map
    m_map.erase( m_streamWin );
}

Vector2i MjpegStream::getPosition() {
    RECT windowPos;

    m_windowMutex.lock();
    GetWindowRect( m_streamWin , &windowPos );
    m_windowMutex.unlock();

    return Vector2i( windowPos.left , windowPos.top );
}

void MjpegStream::setPosition( const Vector2i& position ) {
    m_windowMutex.lock();

    // Set position of stream window
    SetWindowPos( m_streamWin , NULL , position.X , position.Y , getSize().X , getSize().Y , SWP_NOZORDER );

    // Set position of stream button below it
    SetWindowPos( m_toggleButton , NULL , position.X , position.Y + 240 + 5 , 100 , 24 , SWP_NOZORDER );

    m_windowMutex.unlock();
}

Vector2i MjpegStream::getSize() {
    RECT windowPos;

    m_windowMutex.lock();
    GetClientRect( m_streamWin , &windowPos );
    m_windowMutex.unlock();

    return Vector2i( windowPos.right , windowPos.bottom );
}

void MjpegStream::setSize( const Vector2i& size ) {
    m_windowMutex.lock();
    SetWindowPos( m_streamWin , NULL , getPosition().X , getPosition().Y , size.X , size.Y , SWP_NOZORDER );
    m_windowMutex.unlock();

    recreateGraphics( size );
}

void MjpegStream::startStream() {
    if ( m_stopReceive == true ) { // if stream is closed, reopen it
        m_stopReceive = false;
        m_firstImage = true;

        m_imageAge.restart();

        // Launch the MJPEG receiving/processing thread
        m_streamInst = mjpeg_launchthread( const_cast<char*>( m_hostName.c_str() ) , m_port , const_cast<char*>( m_requestPath.c_str() ) , &m_callbacks );

        m_updateThread.launch();

        // Send message to parent window about the stream opening
        PostMessage( m_parentWin , WM_MJPEGSTREAM_START , 0 , 0 );
    }
}

void MjpegStream::stopStream() {
    if ( m_stopReceive == false ) { // if stream is open, close it
        m_stopReceive = true;

        // Close the receive thread
        if ( m_streamInst != NULL ) {
            mjpeg_stopthread( m_streamInst );
            // Send message to parent window about the stream closing
            PostMessage( m_parentWin , WM_MJPEGSTREAM_STOP , 0 , 0 );
        }
    }
}

bool MjpegStream::isStreaming() {
    return !m_stopReceive;
}

void MjpegStream::repaint() {
    m_windowMutex.lock();
    InvalidateRect( m_streamWin , NULL , FALSE );
    m_windowMutex.unlock();
}

void MjpegStream::saveCurrentImage( const std::string& fileName ) {
    m_tempImage.saveToFile( fileName );
}

void MjpegStream::doneCallback( void* optarg ) {
    static_cast<MjpegStream*>(optarg)->m_stopReceive = true;
    static_cast<MjpegStream*>(optarg)->m_streamInst = NULL;
}

void MjpegStream::readCallback( char* buf , int bufsize , void* optarg ) {
    // Create pointer to stream to make it easier to access the instance later
    MjpegStream* streamPtr = static_cast<MjpegStream*>( optarg );

    // Load the image received (converts from JPEG to pixel array)
    bool loadedCorrectly = streamPtr->m_tempImage.loadFromMemory( buf , bufsize );

    if ( loadedCorrectly ) {
        unsigned int length = streamPtr->m_tempImage.getSize().x * streamPtr->m_tempImage.getSize().y * 4;

        streamPtr->m_imageMutex.lock();

        // Allocate new buffer to fit latest image
        delete[] streamPtr->m_pxlBuf;
        streamPtr->m_pxlBuf = new uint8_t[length];

        std::memcpy( streamPtr->m_pxlBuf , streamPtr->m_tempImage.getPixelsPtr() , length );

        streamPtr->m_imgWidth = streamPtr->m_tempImage.getSize().x;
        streamPtr->m_imgHeight = streamPtr->m_tempImage.getSize().y;

        streamPtr->m_imageMutex.unlock();

        // If that was the first image streamed
        if ( streamPtr->m_firstImage ) {
            streamPtr->m_firstImage = false;
        }

        // Reset the image age timer
        streamPtr->m_imageAge.restart();

        // Send message to parent window about the new image
        PostMessage( streamPtr->m_parentWin , WM_MJPEGSTREAM_NEWIMAGE , 0 , 0 );
	}
}

void MjpegStream::recreateGraphics( const Vector2i& windowSize ) {
    // Create new device contexts
    HDC streamWinDC = GetDC( m_streamWin );
    HDC connectDC = CreateCompatibleDC( streamWinDC );
    HDC disconnectDC = CreateCompatibleDC( streamWinDC );
    HDC waitDC = CreateCompatibleDC( streamWinDC );
    HDC backgroundDC = CreateCompatibleDC( streamWinDC );

    // Create a 1:1 relationship between logical units and pixels
    SetMapMode( connectDC , MM_TEXT );
    SetMapMode( disconnectDC , MM_TEXT );
    SetMapMode( waitDC , MM_TEXT );
    SetMapMode( backgroundDC , MM_TEXT );

    // Create the bitmaps used for graphics
    HBITMAP connectBmp = CreateCompatibleBitmap( streamWinDC , getSize().X , getSize().Y );
    HBITMAP disconnectBmp = CreateCompatibleBitmap( streamWinDC , getSize().X , getSize().Y );
    HBITMAP waitBmp = CreateCompatibleBitmap( streamWinDC , getSize().X , getSize().Y );
    HBITMAP backgroundBmp = CreateCompatibleBitmap( streamWinDC , getSize().X , getSize().Y );

    ReleaseDC( m_streamWin , streamWinDC );

    // Give each graphic a bitmap to use
    SelectObject( connectDC , connectBmp );
    SelectObject( disconnectDC , disconnectBmp );
    SelectObject( waitDC , waitBmp );
    SelectObject( backgroundDC , backgroundBmp );

    // Create brushes and regions for backgrounds
    HBRUSH backgroundBrush = CreateSolidBrush( RGB( 255 , 255 , 255 ) );
    HRGN backgroundRegion = CreateRectRgn( 0 , 0 , windowSize.X , windowSize.Y );

    HBRUSH transparentBrush = CreateSolidBrush( RGB( 0 , 0 , 0 ) );

    HBRUSH waitBrush = CreateSolidBrush( RGB( 128 , 128 , 128 ) );
    HRGN waitRegion = CreateRectRgn( windowSize.X / 3 , windowSize.Y / 3 , 2 * windowSize.X / 3 , 2 * windowSize.Y / 3 );

    /* ===== Fill graphics with a background color ===== */
    FillRgn( connectDC , backgroundRegion , backgroundBrush );
    FillRgn( disconnectDC , backgroundRegion , backgroundBrush );

    // Need a special background color since they will be transparent
    FillRgn( waitDC , backgroundRegion , transparentBrush );

    // Add transparent rectangle
    FillRgn( waitDC , waitRegion , waitBrush );

    // Create background with transparency
    FillRgn( waitDC , backgroundRegion , backgroundBrush );
    /* ================================================= */

    // Free the brushes and regions
    DeleteObject( backgroundBrush );
    DeleteObject( backgroundRegion );
    DeleteObject( waitRegion );
    DeleteObject( waitBrush );

    /* ===== Recenter the messages ===== */
    m_connectMsg.setPosition( Vector2i( windowSize.X / 2.f ,
            windowSize.Y / 2.f ) );

    m_disconnectMsg.setPosition( Vector2i( windowSize.X / 2.f ,
            windowSize.Y / 2.f ) );

    m_waitMsg.setPosition( Vector2i( windowSize.X / 2.f ,
            windowSize.Y / 2.f ) );
    /* ================================= */

    /* ===== Fill device contexts with messages ===== */
    int oldBkMode;
    UINT oldAlign;

    oldBkMode = SetBkMode( connectDC , TRANSPARENT );
    oldAlign = SetTextAlign( connectDC , TA_CENTER | TA_BASELINE );
    m_connectMsg.draw( connectDC );
    SetTextAlign( connectDC , oldAlign );
    SetBkMode( connectDC , oldBkMode );

    oldBkMode = SetBkMode( disconnectDC , TRANSPARENT );
    oldAlign = SetTextAlign( disconnectDC , TA_CENTER | TA_BASELINE );
    m_disconnectMsg.draw( disconnectDC );
    SetTextAlign( disconnectDC , oldAlign );
    SetBkMode( disconnectDC , oldBkMode );

    oldBkMode = SetBkMode( waitDC , TRANSPARENT );
    oldAlign = SetTextAlign( waitDC , TA_CENTER | TA_BASELINE );
    m_waitMsg.draw( waitDC );
    SetTextAlign( waitDC , oldAlign );
    SetBkMode( waitDC , oldBkMode );
    /* ============================================== */

    m_imageMutex.lock();
    /* ===== Allocate buffers for pixels of graphics ===== */
    delete[] m_connectPxl;
    m_connectPxl = new BYTE[windowSize.X * windowSize.Y * 4];

    delete[] m_disconnectPxl;
    m_disconnectPxl = new BYTE[windowSize.X * windowSize.Y * 4];

    delete[] m_waitPxl;
    m_waitPxl = new BYTE[windowSize.X * windowSize.Y * 4];

    delete[] m_backgroundPxl;
    m_backgroundPxl = new BYTE[windowSize.X * windowSize.Y * 4];
    /* =================================================== */

    /* ===== Store bits from graphics in another buffer ===== */
    BMPtoPXL( connectDC , connectBmp , windowSize.X , windowSize.Y , m_connectPxl );
    BMPtoPXL( disconnectDC , disconnectBmp , windowSize.X , windowSize.Y , m_disconnectPxl );
    BMPtoPXL( waitDC , waitBmp , windowSize.X , windowSize.Y , m_waitPxl );
    BMPtoPXL( backgroundDC , backgroundBmp , windowSize.X , windowSize.Y , m_backgroundPxl );
    /* ====================================================== */
    m_imageMutex.unlock();

    // Free WinGDI objects
    DeleteDC( connectDC );
    DeleteDC( disconnectDC );
    DeleteDC( waitDC );
    DeleteDC( backgroundDC );
    DeleteObject( connectBmp );
    DeleteObject( disconnectBmp );
    DeleteObject( waitBmp );
    DeleteObject( backgroundBmp );
}

void MjpegStream::EnableOpenGL() {
    // Stores pixel format
    int format;

    // GL error
    GLenum glError;

    // Set the pixel format for the DC
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support window
        PFD_SUPPORT_OPENGL |   // support OpenGL
        PFD_DOUBLEBUFFER,      // double buffered
        PFD_TYPE_RGBA,         // RGBA type
        24,                    // 24-bit color depth
        8, 0, 8, 8, 8, 16,     // 8 bits per color component, evenly spaced
        0,                     // no alpha buffer
        0,                     // shift bit ignored
        0,                     // no accumulation buffer
        0, 0, 0, 0,            // accum bits ignored
        16,                    // 16-bit z-buffer
        0,                     // no stencil buffer
        0,                     // no auxiliary buffer
        PFD_MAIN_PLANE,        // main layer
        0,                     // reserved
        0, 0, 0                // layer masks ignored
    };

    // Get the device context (DC)
    m_bufferDC = GetDC( m_streamWin );

    format = ChoosePixelFormat( m_bufferDC , &pfd );
    SetPixelFormat( m_bufferDC , format , &pfd );

    // Create and enable the render context (RC)
    m_threadRC = wglCreateContext( m_bufferDC );
    wglMakeCurrent( m_bufferDC , m_threadRC );

    /* ===== Initialize OpenGL ===== */
    glClearColor( 0.f , 0.f , 0.f , 0.f );
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

    glTexParameteri( GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , GL_LINEAR );

    // Set up screen
    glViewport( 0 , 0 , getSize().X , getSize().Y );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0 , getSize().X , getSize().Y , 0 , -1.f , 1.f );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    // Check for OpenGL errors
    glError = glGetError();
    if( glError != GL_NO_ERROR ) {
        std::cerr << "Error initializing OpenGL: " << gluErrorString( glError ) << "\n";
    }
    /* ============================= */

    m_imgWidth = getSize().X;
    m_imgHeight = getSize().Y;
}

void MjpegStream::DisableOpenGL() {
    wglMakeCurrent( NULL , NULL );
    wglDeleteContext( m_threadRC );
    ReleaseDC( m_streamWin , m_bufferDC );
}

LRESULT CALLBACK MjpegStream::WindowProc( HWND handle , UINT message , WPARAM wParam , LPARAM lParam ) {
    switch ( message ) {
    case WM_DESTROY: {
        m_map[handle]->DisableOpenGL();

        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint( handle , &ps );

        m_map[handle]->paint( &ps );

        EndPaint( handle , &ps );

        break;
    }

    default: {
        return DefWindowProc( handle , message , wParam , lParam );
    }
    }

    return 0;
}

void MjpegStream::paint( PAINTSTRUCT* ps ) {
    GLenum glError;
    int textureSize;

    /* If our image won't fit in the texture, make a bigger one whose width and
     * height are a power of two.
     */
    if( m_imgWidth > m_dispWidth || m_imgHeight > m_dispHeight ) {
        textureSize = npot( std::max( m_imgWidth , m_imgHeight ) );

        uint8_t* tmpBuf = new uint8_t[textureSize * textureSize * 3];
        glTexImage2D( GL_TEXTURE_2D , 0 , 3 , textureSize , textureSize , 0 ,
                GL_RGB , GL_UNSIGNED_BYTE , tmpBuf );
        delete[] tmpBuf;

        m_dispWidth = textureSize;
        m_dispHeight = textureSize;
    }

    /* Represents the amount of the texture to display. This
     * is a ratio of the actual texture size to the displayed
     * amount. These are passed into glTexCoord2f.
     */
    double wratio = (float)getSize().X / (float)m_dispWidth;
    double hratio = (float)getSize().Y / (float)m_dispHeight;

    // If streaming is enabled
    if ( isStreaming() ) {
        // If no image has been received yet
        if ( m_firstImage ) {
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , m_imgWidth ,
                    m_imgHeight , GL_RGBA , GL_UNSIGNED_BYTE , m_connectPxl );

            m_imageMutex.unlock();
        }

        // If it's been too long since we received our last image
        else if ( m_imageAge.getElapsedTime().asMilliseconds() > 1000 ) {
            // Display "Waiting..." over the last image received
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , m_imgWidth ,
                    m_imgHeight , GL_RGBA , GL_UNSIGNED_BYTE , m_waitPxl );

            // Send message to parent window about the new image
            PostMessage( m_parentWin , WM_MJPEGSTREAM_NEWIMAGE , 0 , 0 );

            m_imageMutex.unlock();
        }

        // Else display the image last received
        else {
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , m_imgWidth ,
                    m_imgHeight, GL_RGBA , GL_UNSIGNED_BYTE , m_pxlBuf );

            m_imageMutex.unlock();
        }
    }

    // Else we aren't connected to the host; display disconnect graphic
    else {
        m_imageMutex.lock();

        glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , m_imgWidth , m_imgHeight,
                GL_RGBA , GL_UNSIGNED_BYTE , m_disconnectPxl );

        m_imageMutex.unlock();
    }

    // Position the GL texture in the window
    glBegin( GL_TRIANGLE_FAN );
    glColor4f( 1.f , 1.f , 1.f , 1.f );
    glTexCoord2f( 0 , 0 ); glVertex3f( 0 , 0 , 0 );
    glTexCoord2f( wratio , 0 ); glVertex3f( m_imgWidth , 0 , 0 );
    glTexCoord2f( wratio , hratio ); glVertex3f( m_imgWidth , m_imgHeight , 0 );
    glTexCoord2f( 0 , hratio ); glVertex3f( 0 , m_imgHeight , 0 );
    glEnd();

    // Check for OpenGL errors
    glError = glGetError();
    if( glError != GL_NO_ERROR ) {
        std::cerr << "Failed to draw texture: " << gluErrorString( glError ) << "\n";
    }

    // Display OpenGL drawing
    SwapBuffers( m_bufferDC );

    char buttonText[13];
    GetWindowText( m_toggleButton , buttonText , 13 );

    // If running and button displays "Start"
    if ( !m_stopReceive && std::strcmp( buttonText , "Start Stream" ) == 0 ) {
        // Change text displayed on button (LParam is button HWND)
        SendMessage( m_toggleButton , WM_SETTEXT , 0 , reinterpret_cast<LPARAM>("Stop Stream") );
    }

    // If not running and button displays "Stop"
    else if ( m_stopReceive && std::strcmp( buttonText , "Stop Stream" ) == 0 ) {
        // Change text displayed on button (LParam is button HWND)
        SendMessage( m_toggleButton , WM_SETTEXT , 0 , reinterpret_cast<LPARAM>("Start Stream") );
    }
}
