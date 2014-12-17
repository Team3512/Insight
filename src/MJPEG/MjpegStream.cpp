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

#include "stb_image.h"
#include "stb_image_write.h"

#include <iostream>
#include <wingdi.h>
#include <windowsx.h>
#include <cstring>

std::map<HWND , MjpegStream*> MjpegStream::m_map;

void BMPtoPXL( HDC dc , HBITMAP bmp , uint8_t* pxlData ) {
    BITMAP tempBmp;
    GetObject( bmp , sizeof(BITMAP) , &tempBmp );

    BITMAPINFOHEADER bmi = {0};
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biWidth = tempBmp.bmWidth;
    bmi.biHeight = -tempBmp.bmHeight;
    bmi.biCompression = BI_RGB;
    bmi.biSizeImage = 0;// 3 * ScreenX * ScreenY; for PNG or JPEG

    GetDIBits( dc , bmp , 0 , tempBmp.bmHeight , pxlData , (BITMAPINFO*)&bmi , DIB_RGB_COLORS );
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
        HINSTANCE appInstance ,
        WindowCallbacks* windowCallbacks ,
        std::function<void(void)> newImageCallback ,
        std::function<void(void)> startCallback ,
        std::function<void(void)> stopCallback
        ) :
        MjpegClient( hostName , port , requestPath ) ,

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

    // Create the textures that can be displayed in the stream window
    recreateGraphics( Vector2i( width , height ) );

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
    m_updateThread = new std::thread( MjpegStream::updateFunc , this );
}

MjpegStream::~MjpegStream() {
    stop();

    m_stopUpdate = true;
    m_updateThread->join();
    delete m_updateThread;

    delete[] m_connectPxl;
    delete[] m_disconnectPxl;
    delete[] m_waitPxl;
    delete[] m_backgroundPxl;

    delete m_glWin;

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

void MjpegStream::start() {
    if ( !isStreaming() ) {
        MjpegClient::start();
        if ( isStreaming() ) {
            m_firstImage = true;
            m_imageAge = std::chrono::system_clock::now();

            repaint();
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
        repaint();
        if ( m_stopCallback != nullptr ) {
            m_stopCallback();
        }
    }
}

void MjpegStream::setFPS( unsigned int fps ) {
    m_frameRate = fps;
}

void MjpegStream::repaint() {
    m_windowMutex.lock();
    InvalidateRect( m_streamWin , NULL , FALSE );
    m_windowMutex.unlock();
}

void MjpegStream::done( void* optarg ) {
    static_cast<MjpegStream*>(optarg)->repaint();
}

void MjpegStream::read( char* buf , int bufsize , void* optarg ) {
    // Create pointer to stream to make it easier to access the instance later
    MjpegStream* streamPtr = static_cast<MjpegStream*>( optarg );

    // Send message to parent window about the new image
    if ( std::chrono::system_clock::now() - streamPtr->m_displayTime > std::chrono::duration<double>(1.0 / streamPtr->m_frameRate) ) {
        repaint();
        if ( m_newImageCallback != nullptr ) {
            m_newImageCallback();
        }
        streamPtr->m_imageAge = std::chrono::system_clock::now();
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

    m_imageMutex.lock();
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
    BMPtoPXL( connectDC , connectBmp , m_connectPxl );
    BMPtoPXL( disconnectDC , disconnectBmp , m_disconnectPxl );
    BMPtoPXL( waitDC , waitBmp , m_waitPxl );
    BMPtoPXL( backgroundDC , backgroundBmp , m_backgroundPxl );
    /* ====================================================== */
    m_imageMutex.unlock();

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

void MjpegStream::updateFunc() {
    std::chrono::duration<double> lastTime( 0.0 );
    std::chrono::duration<double> currentTime( 0.0 );

    while ( isStreaming() ) {
        currentTime = std::chrono::system_clock::now() - m_imageAge;

        // Make "Waiting..." graphic show up
        if ( currentTime > std::chrono::milliseconds(1000) && lastTime <= std::chrono::milliseconds(1000) ) {
            repaint();
        }

        lastTime = currentTime;

        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    }
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
    m_glWin->makeContextCurrent();
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
        // Check for new image
        if ( newImageAvailable() ) {
            m_imageMutex.lock();

            m_img = getCurrentImage();

            Vector2i size = getCurrentSize();
            m_imgWidth = size.X;
            m_imgHeight = size.Y;

            m_imageMutex.unlock();

            if ( m_firstImage ) {
                m_firstImage = false;
            }

            // Reset "Waiting..." timeout
            m_imageAge = std::chrono::system_clock::now();
        }

        // If no image has been received yet
        if ( m_firstImage ) {
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , getSize().X ,
                    getSize().Y , GL_RGBA , GL_UNSIGNED_BYTE , m_connectPxl );

            m_imageMutex.unlock();

            wratio = (float)getSize().X / (float)m_textureWidth;
            hratio = (float)getSize().Y / (float)m_textureHeight;
        }

        // If it's been too long since we received our last image
        else if ( std::chrono::system_clock::now() - m_imageAge > std::chrono::milliseconds(1000) ) {
            // Display "Waiting..." over the last image received
            m_imageMutex.lock();

            glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , getSize().X ,
                    getSize().Y , GL_RGBA , GL_UNSIGNED_BYTE , m_waitPxl );

            m_imageMutex.unlock();

            wratio = (float)getSize().X / (float)m_textureWidth;
            hratio = (float)getSize().Y / (float)m_textureHeight;
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

        glTexSubImage2D( GL_TEXTURE_2D , 0 , 0 , 0 , getSize().X , getSize().Y ,
                GL_RGBA , GL_UNSIGNED_BYTE , m_disconnectPxl );

        m_imageMutex.unlock();

        wratio = (float)getSize().X / (float)m_textureWidth;
        hratio = (float)getSize().Y / (float)m_textureHeight;
    }

    // Position the GL texture in the Win32 window
    glBegin( GL_TRIANGLE_FAN );
    glColor4f( 1.f , 1.f , 1.f , 1.f );
    glTexCoord2f( 0 , 0 ); glVertex2f( 0 , 0 );
    glTexCoord2f( wratio , 0 ); glVertex2f( getSize().X , 0 );
    glTexCoord2f( wratio , hratio ); glVertex2f( getSize().X , getSize().Y );
    glTexCoord2f( 0 , hratio ); glVertex2f( 0 , getSize().Y );
    glEnd();

    // Check for OpenGL errors
    glError = glGetError();
    if( glError != GL_NO_ERROR ) {
        std::cerr << "Failed to draw texture: " << gluErrorString( glError ) << "\n";
    }

    // Display OpenGL drawing
    m_glWin->swapBuffers();

    m_displayTime = std::chrono::system_clock::now();

    char buttonText[13];
    GetWindowText( m_toggleButton , buttonText , 13 );

    // If running and button displays "Start"
    if ( isStreaming() && std::strcmp( buttonText , "Start Stream" ) == 0 ) {
        // Change text displayed on button (LParam is button HWND)
        SendMessage( m_toggleButton , WM_SETTEXT , 0 , reinterpret_cast<LPARAM>("Stop Stream") );
    }

    // If not running and button displays "Stop"
    else if ( !isStreaming() && std::strcmp( buttonText , "Stop Stream" ) == 0 ) {
        // Change text displayed on button (LParam is button HWND)
        SendMessage( m_toggleButton , WM_SETTEXT , 0 , reinterpret_cast<LPARAM>("Start Stream") );
    }
}
