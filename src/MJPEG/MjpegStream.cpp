//=============================================================================
//File Name: MjpegStream.cpp
//Description: Receives an MJPEG stream and displays it in a child window with
//             the specified properties
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "../WinGDI/UIFont.hpp"
#include "../Util.hpp"
#include "../Resource.h"
#include "MjpegStream.hpp"
#include "mjpeg_sleep.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <iostream>
#include <wingdi.h>
#include <windowsx.h>
#include <cstring>

std::map<HWND , MjpegStream*> MjpegStream::m_map;

void BMPtoPXL( HDC dc , HBITMAP bmp , int width , int height , uint8_t* pxlData ) {
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

// Convert a string to lower case
std::string toLower( std::string str ) {
    for ( std::string::iterator i = str.begin() ; i != str.end() ; ++i ) {
        *i = static_cast<char>(std::tolower(*i));
    }
    return str;
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
        HINSTANCE appInstance,
        WindowCallbacks* windowCallbacks
        ) :
        m_hostName( hostName ) ,
        m_port( port ) ,
        m_requestPath( requestPath ) ,

        m_connectMsg( Vector2i( 0 , 0 ) , UIFont::getInstance().segoeUI18() ,
                L"Connecting..." , Colorf( 0 , 0 , 0 ) , Colorf( 255 , 255 , 255 ) ) ,
        m_connectPxl( NULL ) ,
        m_disconnectMsg( Vector2i( 0 , 0 ) , UIFont::getInstance().segoeUI18() ,
                L"Disconnected" , Colorf( 0 , 0 , 0 ) , Colorf( 255 , 255 , 255 ) ) ,
        m_disconnectPxl( NULL ) ,
        m_waitMsg( Vector2i( 0 , 0 ) , UIFont::getInstance().segoeUI18() ,
                L"Waiting..." , Colorf( 0 , 0 , 0 ) , Colorf( 255 , 255 , 255 ) ) ,
        m_waitPxl( NULL ) ,
        m_backgroundPxl( NULL ) ,

        m_pxlBuf( NULL ) ,
        m_imgWidth( 0 ) ,
        m_imgHeight( 0 ) ,
        m_textureWidth( 0 ) ,
        m_textureHeight( 0 ) ,

        m_extBuf( NULL ) ,
        m_extWidth( 0 ) ,
        m_extHeight( 0 ) ,

        m_newImageAvailable( false ) ,

        m_firstImage( true ) ,

        m_streamInst( NULL ) ,

        m_frameRate( 15 ) ,

        m_stopReceive( true ) ,
        m_stopUpdate( true )
{
    // Initialize the WindowCallbacks pointer
    m_windowCallbacks = windowCallbacks;

    // Initialize the parent window handle
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
        xPosition + 5,
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

    // Initialize mutexes
    mjpeg_mutex_init( &m_imageMutex );
    mjpeg_mutex_init( &m_extMutex );
    mjpeg_mutex_init( &m_windowMutex );

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
    m_glWin = new GLWindow( m_streamWin );

    m_imgWidth = getSize().X;
    m_imgHeight = getSize().Y;

    m_stopUpdate = false;
    if ( mjpeg_thread_create( &m_updateThread , &MjpegStream::updateFunc , this ) == -1 ) {
        m_stopUpdate = true;
    }
}

MjpegStream::~MjpegStream() {
    stopStream();

    m_stopUpdate = true;
    mjpeg_thread_join( &m_updateThread , NULL );

    delete[] m_connectPxl;
    delete[] m_disconnectPxl;
    delete[] m_waitPxl;
    delete[] m_backgroundPxl;

    delete[] m_pxlBuf;
    delete[] m_extBuf;

    // Destroy mutexes
    mjpeg_mutex_destroy( &m_imageMutex );
    mjpeg_mutex_destroy( &m_extMutex );
    mjpeg_mutex_destroy( &m_windowMutex );

    delete m_glWin;

    DestroyWindow( m_streamWin );
    DestroyWindow( m_toggleButton );

    // Remove window from global map
    m_map.erase( m_streamWin );
}

Vector2i MjpegStream::getPosition() {
    RECT windowPos;

    mjpeg_mutex_lock( &m_windowMutex );
    GetWindowRect( m_streamWin , &windowPos );
    mjpeg_mutex_unlock( &m_windowMutex );

    return Vector2i( windowPos.left , windowPos.top );
}

void MjpegStream::setPosition( const Vector2i& position ) {
    mjpeg_mutex_lock( &m_windowMutex );

    // Set position of stream window
    SetWindowPos( m_streamWin , NULL , position.X , position.Y , getSize().X , getSize().Y , SWP_NOZORDER );

    // Set position of stream button below it
    SetWindowPos( m_toggleButton , NULL , position.X , position.Y + 240 + 5 , 100 , 24 , SWP_NOZORDER );

    mjpeg_mutex_unlock( &m_windowMutex );
}

Vector2i MjpegStream::getSize() {
    RECT windowPos;

    mjpeg_mutex_lock( &m_windowMutex );
    GetClientRect( m_streamWin , &windowPos );
    mjpeg_mutex_unlock( &m_windowMutex );

    return Vector2i( windowPos.right , windowPos.bottom );
}

void MjpegStream::setSize( const Vector2i& size ) {
    mjpeg_mutex_lock( &m_windowMutex );
    SetWindowPos( m_streamWin , NULL , getPosition().X , getPosition().Y , size.X , size.Y , SWP_NOZORDER );
    mjpeg_mutex_unlock( &m_windowMutex );

    recreateGraphics( size );
}

void MjpegStream::startStream() {
    if ( m_stopReceive == true ) { // if stream is closed, reopen it
        m_stopReceive = false;
        m_firstImage = true;

        m_imageAge = std::chrono::system_clock::now();

        // Launch the MJPEG receiving/processing thread
        m_streamInst = mjpeg_launchthread( const_cast<char*>( m_hostName.c_str() ) , m_port , const_cast<char*>( m_requestPath.c_str() ) , &m_callbacks );
        if ( m_streamInst == NULL ) {
            m_stopReceive = true;
        }
        else {
            // Send message to parent window about the stream opening
            PostMessage( m_parentWin , WM_MJPEGSTREAM_START , 0 , 0 );
        }
    }
}

void MjpegStream::stopStream() {
    if ( m_stopReceive == false ) { // if stream is open, close it
        m_stopReceive = true;

        // Close the receive thread
        if ( m_streamInst != NULL ) {
            mjpeg_stopthread( m_streamInst );
        }

        // Send message to parent window about the stream closing
        PostMessage( m_parentWin , WM_MJPEGSTREAM_STOP , 0 , 0 );
    }
}

bool MjpegStream::isStreaming() {
    return !m_stopReceive;
}

void MjpegStream::setFPS( unsigned int fps ) {
    m_frameRate = fps;
}

void MjpegStream::repaint() {
    mjpeg_mutex_lock( &m_windowMutex );
    InvalidateRect( m_streamWin , NULL , FALSE );
    mjpeg_mutex_unlock( &m_windowMutex );
}

void MjpegStream::saveCurrentImage( const std::string& fileName ) {
    mjpeg_mutex_lock( &m_imageMutex );

    // Deduce the image type from its extension
    if ( fileName.size() > 3 ) {
        // Extract the extension
        std::string extension = fileName.substr(fileName.size() - 3);

        if ( toLower(extension) == "bmp" ) {
            // BMP format
            stbi_write_bmp( fileName.c_str(), m_imgWidth, m_imgHeight, 4, m_pxlBuf );
        }
        else if ( toLower(extension) == "tga" ) {
            // TGA format
            stbi_write_tga( fileName.c_str(), m_imgWidth, m_imgHeight, 4, m_pxlBuf );
        }
        else if( toLower(extension) == "png" ) {
            // PNG format
            stbi_write_png( fileName.c_str(), m_imgWidth, m_imgHeight, 4, m_pxlBuf, 0 );
        }
        else {
            std::cout << "MjpegStream: failed to save image to '" << fileName << "'\n";
        }
    }

    mjpeg_mutex_unlock( &m_imageMutex );
}

uint8_t* MjpegStream::getCurrentImage() {
    mjpeg_mutex_lock( &m_imageMutex );
    mjpeg_mutex_lock( &m_extMutex );

    if ( m_pxlBuf != NULL ) {
        // If buffer is wrong size, reallocate it
        if ( m_imgWidth != m_extWidth || m_imgHeight != m_extHeight ) {
            if ( m_extBuf != NULL ) {
                delete[] m_extBuf;
            }

            // Allocate new buffer to fit latest image
            m_extBuf = new uint8_t[m_imgWidth * m_imgHeight * 4];
            m_extWidth = m_imgWidth;
            m_extHeight = m_imgHeight;
        }

        std::memcpy( m_extBuf , m_pxlBuf , m_extWidth * m_extHeight * 4 );

        /* Since the image just got copied, it's no longer new. This is checked
         * in the if statement "m_pxlBuf != NULL" because it's impossible for
         * this to be set to true when no image has been received yet.
         */
        m_newImageAvailable = false;
    }

    mjpeg_mutex_unlock( &m_extMutex );
    mjpeg_mutex_unlock( &m_imageMutex );

    return m_extBuf;
}

Vector2i MjpegStream::getCurrentSize() {
    mjpeg_mutex_lock( &m_extMutex );

    Vector2i temp( m_extWidth , m_extHeight );

    mjpeg_mutex_unlock( &m_extMutex );

    return temp;
}

bool MjpegStream::newImageAvailable() {
    return m_newImageAvailable;
}

void MjpegStream::doneCallback( void* optobj ) {
    static_cast<MjpegStream*>(optobj)->m_stopReceive = true;
    static_cast<MjpegStream*>(optobj)->m_streamInst = NULL;
    static_cast<MjpegStream*>(optobj)->repaint();
}

void MjpegStream::readCallback( char* buf , int bufsize , void* optobj ) {
    // Create pointer to stream to make it easier to access the instance later
    MjpegStream* streamPtr = static_cast<MjpegStream*>( optobj );

    // Load the image received (converts from JPEG to pixel array)
    int width, height, channels;
    uint8_t* ptr = stbi_load_from_memory((unsigned char*)buf, bufsize, &width, &height, &channels, STBI_rgb_alpha);

    if ( ptr && width && height ) {
        mjpeg_mutex_lock( &streamPtr->m_imageMutex );

        // Free old buffer and store new one created by stbi_load_from_memory()
        delete[] streamPtr->m_pxlBuf;

        streamPtr->m_pxlBuf = ptr;

        streamPtr->m_imgWidth = width;
        streamPtr->m_imgHeight = height;

        mjpeg_mutex_unlock( &streamPtr->m_imageMutex );

        // If that was the first image streamed
        if ( streamPtr->m_firstImage ) {
            streamPtr->m_firstImage = false;
        }

        if ( !streamPtr->m_newImageAvailable ) {
            streamPtr->m_newImageAvailable = true;
        }

        // Reset the image age timer
        streamPtr->m_imageAge = std::chrono::system_clock::now();

        // Send message to parent window about the new image
        if ( std::chrono::system_clock::now() - streamPtr->m_displayTime > std::chrono::duration<double>(1.0 / streamPtr->m_frameRate) ) {
            PostMessage( streamPtr->m_parentWin , WM_MJPEGSTREAM_NEWIMAGE , 0 , 0 );
            streamPtr->m_displayTime = std::chrono::system_clock::now();
        }
    }
    else {
        std::cout << "MjpegStream: image failed to load: " << stbi_failure_reason() << "\n";
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
    HBITMAP oldConnectBmp = static_cast<HBITMAP>(SelectObject( connectDC , connectBmp ));
    HBITMAP oldDisconnectBmp = static_cast<HBITMAP>(SelectObject( disconnectDC , disconnectBmp ));
    HBITMAP oldWaitBmp = static_cast<HBITMAP>(SelectObject( waitDC , waitBmp ));
    HBITMAP oldBackgroundBmp = static_cast<HBITMAP>(SelectObject( backgroundDC , backgroundBmp ));

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
    DeleteObject( transparentBrush );
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

    mjpeg_mutex_lock( &m_imageMutex );
    /* ===== Allocate buffers for pixels of graphics ===== */
    delete[] m_connectPxl;
    m_connectPxl = new uint8_t[windowSize.X * windowSize.Y * 4];

    delete[] m_disconnectPxl;
    m_disconnectPxl = new uint8_t[windowSize.X * windowSize.Y * 4];

    delete[] m_waitPxl;
    m_waitPxl = new uint8_t[windowSize.X * windowSize.Y * 4];

    delete[] m_backgroundPxl;
    m_backgroundPxl = new uint8_t[windowSize.X * windowSize.Y * 4];
    /* =================================================== */

    /* ===== Store bits from graphics in another buffer ===== */
    BMPtoPXL( connectDC , connectBmp , windowSize.X , windowSize.Y , m_connectPxl );
    BMPtoPXL( disconnectDC , disconnectBmp , windowSize.X , windowSize.Y , m_disconnectPxl );
    BMPtoPXL( waitDC , waitBmp , windowSize.X , windowSize.Y , m_waitPxl );
    BMPtoPXL( backgroundDC , backgroundBmp , windowSize.X , windowSize.Y , m_backgroundPxl );
    /* ====================================================== */
    mjpeg_mutex_unlock( &m_imageMutex );

    // Put old bitmaps back in DCs before deleting them
    SelectObject( connectDC , oldConnectBmp );
    SelectObject( disconnectDC , oldDisconnectBmp );
    SelectObject( waitDC , oldWaitBmp );
    SelectObject( backgroundDC , oldBackgroundBmp );

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

void* MjpegStream::updateFunc( void* obj ) {
    std::chrono::duration<double> lastTime( 0.0 );
    std::chrono::duration<double> currentTime( 0.0 );

    while ( !static_cast<MjpegStream*>(obj)->m_stopUpdate ) {
        currentTime = std::chrono::system_clock::now() - static_cast<MjpegStream*>(obj)->m_imageAge;

        // Make "Waiting..." graphic show up
        if ( currentTime > std::chrono::milliseconds(1000) && lastTime <= std::chrono::milliseconds(1000) ) {
            static_cast<MjpegStream*>(obj)->repaint();
        }

        lastTime = currentTime;

        mjpeg_sleep( 50 );
    }

    mjpeg_thread_exit(NULL);
}

LRESULT CALLBACK MjpegStream::WindowProc( HWND handle , UINT message , WPARAM wParam , LPARAM lParam ) {
    switch ( message ) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint( handle , &ps );

        m_map[handle]->paint( &ps );

        EndPaint( handle , &ps );

        break;
    }

    case WM_MOUSEMOVE: {
        /* Mouse moved */
        m_map[handle]->m_lx = GET_X_LPARAM(lParam);
        m_map[handle]->m_ly = GET_Y_LPARAM(lParam);

        break;
    }

    case WM_LBUTTONDOWN: {
        // Button clicked
        m_map[handle]->m_cx = m_map[handle]->m_lx;
        m_map[handle]->m_cy = m_map[handle]->m_ly;
        m_map[handle]->m_windowCallbacks->clickEvent(m_map[handle]->m_cx, m_map[handle]->m_cy);

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
    m_glWin->makeBufferCurrent();
    m_glWin->initGL( getSize().X , getSize().Y );

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
            mjpeg_mutex_lock( &m_imageMutex );

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , getSize().X ,
                    getSize().Y , GL_RGBA , GL_UNSIGNED_BYTE , m_connectPxl );

            mjpeg_mutex_unlock( &m_imageMutex );

            wratio = (float)getSize().X / (float)m_textureWidth;
            hratio = (float)getSize().Y / (float)m_textureHeight;
        }

        // If it's been too long since we received our last image
        else if ( std::chrono::system_clock::now() - m_imageAge > std::chrono::milliseconds(1000) ) {
            // Display "Waiting..." over the last image received
            mjpeg_mutex_lock( &m_imageMutex );

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , getSize().X ,
                    getSize().Y , GL_RGBA , GL_UNSIGNED_BYTE , m_waitPxl );

            // Send message to parent window about the new image
            PostMessage( m_parentWin , WM_MJPEGSTREAM_NEWIMAGE , 0 , 0 );

            mjpeg_mutex_unlock( &m_imageMutex );

            wratio = (float)getSize().X / (float)m_textureWidth;
            hratio = (float)getSize().Y / (float)m_textureHeight;
        }

        // Else display the image last received
        else {
            mjpeg_mutex_lock( &m_imageMutex );

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , m_imgWidth ,
                    m_imgHeight, GL_RGBA , GL_UNSIGNED_BYTE , m_pxlBuf );

            mjpeg_mutex_unlock( &m_imageMutex );

            wratio = (float)m_imgWidth / (float)m_textureWidth;
            hratio = (float)m_imgHeight / (float)m_textureHeight;
        }
    }

    // Else we aren't connected to the host; display disconnect graphic
    else {
        mjpeg_mutex_lock( &m_imageMutex );

        glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , getSize().X , getSize().Y ,
                GL_RGBA , GL_UNSIGNED_BYTE , m_disconnectPxl );

        mjpeg_mutex_unlock( &m_imageMutex );

        wratio = (float)getSize().X / (float)m_textureWidth;
        hratio = (float)getSize().Y / (float)m_textureHeight;
    }

    // Position the GL texture in the Win32 window
    glBegin( GL_TRIANGLE_FAN );
    glColor4f( 1.f , 1.f , 1.f , 1.f );
    glTexCoord2f( 0 , 0 ); glVertex3f( 0 , 0 , 0 );
    glTexCoord2f( wratio , 0 ); glVertex3f( getSize().X , 0 , 0 );
    glTexCoord2f( wratio , hratio ); glVertex3f( getSize().X , getSize().Y , 0 );
    glTexCoord2f( 0 , hratio ); glVertex3f( 0 , getSize().Y , 0 );
    glEnd();

    // Check for OpenGL errors
    glError = glGetError();
    if( glError != GL_NO_ERROR ) {
        std::cerr << "Failed to draw texture: " << gluErrorString( glError ) << "\n";
    }

    // Display OpenGL drawing
    m_glWin->swapBuffers();

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
