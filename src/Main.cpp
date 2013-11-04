//=============================================================================
//File Name: Main.cpp
//Description: Receives images from the robot, processes them, then forwards
//             them to the DriverStationDisplay
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <jpeglib.h>

#include "SFML/Network/IpAddress.hpp"
#include "SFML/Network/UdpSocket.hpp"

#include "SFML/System/Clock.hpp"

#include "MJPEG/MjpegStream.hpp"
#include "MJPEG/MjpegServer.hpp"
#include "Settings.hpp"
#include "Resource.h"

#include "ImageProcess/FindTarget2013.hpp"

#define _WIN32_IE 0x0400
#include <commctrl.h>

// Global because IP configuration settings are needed in CALLBACK OnEvent
Settings gSettings( "IPSettings.txt" );

// Allows manipulation of MjpegStream in CALLBACK OnEvent
MjpegStream* gStreamWinPtr = NULL;
MjpegServer* gServer = NULL;
std::atomic<int> gJpegQuality( 100 );

// Allows usage of socket in CALLBACK OnEvent
sf::UdpSocket* gCtrlSocketPtr = NULL;

LRESULT CALLBACK OnEvent( HWND handle , UINT message , WPARAM wParam , LPARAM lParam );
BOOL CALLBACK AboutCbk( HWND hDlg , UINT message , WPARAM wParam , LPARAM lParam );

/* Compresses RGB image into JPEG. Variables with the 'image_' prefix are for
 * the input image while variables with the 'output_' prefix are for the
 * outputted JPEG. 'quality' is the JPEG compression quality factor.
 */
void RGBtoJPEG( uint8_t** output_buf , unsigned long int* output_len ,
        int quality , uint8_t* image_data , uint32_t image_width ,
        uint32_t image_height ) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer; // pointer to start of scanline (row of image)

    /* Step 1: allocate and initialize JPEG compression object */

    // Set up the error handler
    cinfo.err = jpeg_std_error( &jerr );

    // Initialize the JPEG compression object
    jpeg_create_compress( &cinfo );

    /* Step 2: specify data destination (eg, a file) */
    // Note: steps 2 and 3 can be done in either order

    /* Here we use the library-supplied code to send compressed data to a
     * memory buffer.
     */
    jpeg_mem_dest( &cinfo , output_buf , output_len );

    /* Step 3: set parameters for compression */

    /* First we supply a description of the input image. Four fields of the
     * cinfo struct must be filled in:
     */
    cinfo.image_width = image_width;     // image width, in pixels
    cinfo.image_height = image_height;   // image height, in pixels
    cinfo.input_components = 3;          // # of color components per pixel
    cinfo.in_color_space = JCS_EXT_BGR;  // colorspace of input image

    // Use the library's routine to set default compression parameters
    jpeg_set_defaults( &cinfo );

    // Set any non-default parameters
    jpeg_set_quality( &cinfo , quality , TRUE /* limit to baseline-JPEG values */);

    /* Step 4: Start compressor */

    // TRUE ensures that we will write a complete interchange-JPEG file
    jpeg_start_compress( &cinfo , TRUE );

    /* Step 5: while (scan lines remain to be written)
     *           jpeg_write_scanlines(...);
     */

    /* Here we use the library's state variable cinfo.next_scanline as the loop
    * counter, so that we don't have to keep track ourselves.
    */
    while ( cinfo.next_scanline < cinfo.image_height ) {
        row_pointer = image_data + cinfo.next_scanline * cinfo.image_width *
                cinfo.input_components;
        (void) jpeg_write_scanlines( &cinfo , &row_pointer , 1 );
    }

    /* Step 6: Finish compression */
    jpeg_finish_compress( &cinfo );

    /* Step 7: release JPEG compression object */
    jpeg_destroy_compress( &cinfo );
}

INT WINAPI WinMain( HINSTANCE Instance , HINSTANCE , LPSTR , INT ) {
    INITCOMMONCONTROLSEX icc;

    // Initialize common controls
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    const char* mainClassName = "Insight";

    HICON mainIcon = LoadIcon( Instance , "mainIcon" );
    HMENU mainMenu = LoadMenu( Instance , "mainMenu" );

    // Define a class for our main window
    WNDCLASSEX WindowClass;
    ZeroMemory( &WindowClass , sizeof(WNDCLASSEX) );
    WindowClass.cbSize        = sizeof(WNDCLASSEX);
    WindowClass.style         = 0;
    WindowClass.lpfnWndProc   = &OnEvent;
    WindowClass.cbClsExtra    = 0;
    WindowClass.cbWndExtra    = 0;
    WindowClass.hInstance     = Instance;
    WindowClass.hIcon         = mainIcon;
    WindowClass.hCursor       = LoadCursor( NULL , IDC_ARROW );
    WindowClass.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    WindowClass.lpszMenuName  = NULL;
    WindowClass.lpszClassName = mainClassName;
    WindowClass.hIconSm       = mainIcon;
    RegisterClassEx(&WindowClass);

    MSG message;


    RECT winSize = { 0 , 0 , static_cast<int>(320) , static_cast<int>(278) }; // set the size, but not the position
    AdjustWindowRect(
            &winSize ,
            WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX | WS_CLIPCHILDREN ,
            TRUE ); // window has menu

    // Create a new window to be used for the lifetime of the application
    HWND mainWindow = CreateWindowEx( 0 ,
            mainClassName ,
            "Insight" ,
            WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX | WS_CLIPCHILDREN ,
            ( GetSystemMetrics(SM_CXSCREEN) - (winSize.right - winSize.left) ) / 2 ,
            ( GetSystemMetrics(SM_CYSCREEN) - (winSize.bottom - winSize.top) ) / 2 ,
            winSize.right - winSize.left ,
            winSize.bottom - winSize.top ,
            NULL ,
            mainMenu ,
            Instance ,
            NULL );

    /* If this isn't allocated on the heap, it can't be destroyed soon enough.
     * If it were allocated on the stack, it would be destroyed when it leaves
     * WinMain's scope, which is after its parent window is destroyed. This
     * causes the cleanup in this object's destructor to not complete
     * successfully.
     */
    gStreamWinPtr = new MjpegStream( gSettings.getValueFor( "streamHost" ) ,
            std::atoi( gSettings.getValueFor( "streamPort" ).c_str() ) ,
            gSettings.getValueFor( "streamRequestPath" ) ,
            mainWindow ,
            0 ,
            0 ,
            320 ,
            240 ,
            Instance );

    gServer = new MjpegServer( std::atoi( gSettings.getValueFor(
            "streamServerPort" ).c_str() ) );

    /* ===== Robot Data Sending Variables ===== */
    sf::UdpSocket robotControl;
    robotControl.setBlocking( false );
    gCtrlSocketPtr = &robotControl;

    /* Used for sending control packets to robot
     * data format:
     * 8 byte header
     *     "ctrl\r\n\0\0"
     * 6 bytes of x-y pairs
     *     char x
     *     char y
     *     char x
     *     char y
     *     char x
     *     char y
     * 2 empty bytes
     */
    char data[16] = "ctrl\r\n\0\0";
    std::memset( data + 8 , 0 , sizeof(data) - 8 );
    bool newData = false;

    sf::IpAddress robotIP( gSettings.getValueFor( "robotIP" ) );
    unsigned short robotControlPort = std::atoi( gSettings.getValueFor( "robotControlPort" ).c_str() );

    // Make sure control data isn't sent too fast
    sf::Clock sendTime;
    /* ======================================== */

    /* ===== Image Processing Variables ===== */
    uint8_t* imgBuffer = NULL;
    uint8_t* tempImg = NULL;
    uint32_t imgWidth = 0;
    uint32_t imgHeight = 0;
    uint32_t lastWidth = 0;
    uint32_t lastHeight = 0;
    FindTarget2013 processor;
    /* ====================================== */

    uint8_t* serveImg = NULL;
    unsigned long int serveLen = 0;

    // Make sure the main window is shown before continuing
    ShowWindow( mainWindow , SW_SHOW );

    bool isExiting = false;
    while ( !isExiting ) {
        if ( PeekMessage( &message , NULL , 0 , 0 , PM_REMOVE ) ) {
            if ( message.message != WM_QUIT ) {
                // If a message was waiting in the message queue, process it
                TranslateMessage( &message );
                DispatchMessage( &message );
            }
            else {
                isExiting = true;
            }
        }
        else {
            // Get new image to process
            imgBuffer = gStreamWinPtr->getCurrentImage();
            imgWidth = gStreamWinPtr->getCurrentSize().X;
            imgHeight = gStreamWinPtr->getCurrentSize().Y;

            if ( imgBuffer != NULL && imgWidth > 0 && imgHeight > 0 ) {
                if ( tempImg == NULL ) {
                    tempImg = new uint8_t[imgWidth * imgHeight * 3];
                }
                else if ( lastWidth != imgWidth || lastHeight != imgHeight ) {
                    delete[] tempImg;
                    tempImg = new uint8_t[imgWidth * imgHeight * 3];
                }

                /* ===== Convert RGBA image to BGR for OpenCV ===== */
                // Copy R, G, and B channels but ignore A channel
                for ( unsigned int posIn = 0, posOut = 0 ; posIn < imgWidth * imgHeight ; posIn++, posOut++ ) {
                    // Copy bytes of pixel into corresponding channels
                    tempImg[3*posOut+0] = imgBuffer[4*posIn+2];
                    tempImg[3*posOut+1] = imgBuffer[4*posIn+1];
                    tempImg[3*posOut+2] = imgBuffer[4*posIn+0];
                }
                /* ================================================ */

                // Process the new image
                processor.setImage( tempImg , imgWidth , imgHeight );
                processor.processImage();

                processor.getProcessedImage( tempImg );

                std::free( serveImg );
                serveImg = NULL;
                serveLen = 0;

                // Convert RGB image to JPEG
                RGBtoJPEG( &serveImg , &serveLen , gJpegQuality , tempImg , imgWidth , imgHeight );
                gServer->serveImage( serveImg , serveLen );

                // Retrieve positions of targets and send them to robot
                if ( processor.getTargetPositions().size() > 0 ) {
                    char x = 0;
                    char y = 0;

                    // Pack data structure with points
                    for ( unsigned int i = 0 ; i < processor.getTargetPositions().size() &&
                            i < 3 ; i++ ) {
                        quad_t target = processor.getTargetPositions()[i];
                        for ( unsigned int j = 0 ; j < 4 ; j++ ) {
                            x += target.point[j].x;
                            y += target.point[j].y;
                        }
                        x /= 4;
                        y /= 4;

                        data[9 + 2*i] = x;
                        data[10 + 2*i] = y;
                    }

                    /* If there are less than three points in the data
                     * structure, zero the rest out.
                     */
                    for ( unsigned int i = processor.getTargetPositions().size() ;
                            i < 3 ; i++ ) {
                        data[9 + 2*i] = 0;
                        data[10 + 2*i] = 0;
                    }

                    // We have new target data to send to the robot
                    newData = true;
                }

                // Update width and height
                lastWidth = imgWidth;
                lastHeight = imgHeight;
            }

            if ( gCtrlSocketPtr != NULL &&
                    sendTime.getElapsedTime().asMilliseconds() > 200 &&
                    newData ) {
                sf::Socket::Status status = gCtrlSocketPtr->send( data , sizeof(data) , robotIP , robotControlPort );
                if ( status == sf::Socket::Done ) {
                    sendTime.restart();
                }
            }

            // Make the window redraw the controls
            InvalidateRect( mainWindow , NULL , FALSE );

            Sleep( 100 );
        }
    }

    // Delete MJPEG stream window and server
    delete gStreamWinPtr;
    delete gServer;

    delete[] tempImg;
    std::free( serveImg );

    // Clean up windows
    DestroyWindow( mainWindow );
    UnregisterClass( mainClassName , Instance );

    return message.wParam;
}

LRESULT CALLBACK OnEvent( HWND handle , UINT message , WPARAM wParam , LPARAM lParam ) {
    switch ( message ) {
    case WM_CREATE: {
        RECT winSize;
        GetClientRect( handle , &winSize );

        // Create slider that controls JPEG quality of MJPEG server
        HWND slider = CreateWindowEx( 0 ,
                TRACKBAR_CLASS ,
                "JPEG Quality" ,
                WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_BOTTOM | TBS_TOOLTIPS ,
                (winSize.right - winSize.left) - 100 ,
                (winSize.bottom - winSize.top) - 32 ,
                100 ,
                32 ,
                handle ,
                NULL ,
                GetModuleHandle( NULL ) ,
                NULL );

        // Make one tick for every two values
        SendMessage( slider , TBM_SETTICFREQ , (WPARAM)20 , (LPARAM)0 );

        // Set initial server JPEG quality to 100
        SendMessage( slider , TBM_SETPOS , (WPARAM)TRUE , (LPARAM)100 );

        break;
    }
    case WM_COMMAND: {
        /* TODO Use these if necessary
        sf::IpAddress robotIP( gSettings.getValueFor( "robotIP" ) );
        unsigned short robotDataPort = std::atoi( gSettings.getValueFor( "robotControlPort" ).c_str() );
        */

        switch( LOWORD(wParam) ) {
            case IDC_STREAM_BUTTON: {
                 if ( gStreamWinPtr != NULL ) {
                     if ( gStreamWinPtr->isStreaming() ) {
                         // Stop streaming
                         if ( gServer != NULL ) {
                             gServer->stop();
                         }
                         gStreamWinPtr->stopStream();
                     }
                     else {
                         // Start streaming
                         gStreamWinPtr->startStream();
                         if ( gServer != NULL ) {
                             gServer->start();
                         }
                     }
                 }

                 break;
            }
            case IDM_SERVER_START: {
                if ( gStreamWinPtr != NULL ) {
                    if ( !gStreamWinPtr->isStreaming() ) {
                        // Start streaming
                        gStreamWinPtr->startStream();
                        if ( gServer != NULL ) {
                            gServer->start();
                        }
                    }
                }

                break;
            }
            case IDM_SERVER_STOP: {
                if ( gStreamWinPtr != NULL ) {
                    if ( gStreamWinPtr->isStreaming() ) {
                        // Stop streaming
                        if ( gServer != NULL ) {
                            gServer->stop();
                        }
                        gStreamWinPtr->stopStream();
                    }
                }

                break;
            }

            case IDM_ABOUT: {
                DialogBox( GetModuleHandle(NULL) , MAKEINTRESOURCE(IDD_ABOUTBOX) , handle , AboutCbk );

                break;
            }
        }

        break;
    }

    case WM_HSCROLL: {
        switch ( LOWORD(wParam) ) {
        case SB_ENDSCROLL: {
            gJpegQuality = SendMessage( (HWND)lParam , TBM_GETPOS , (WPARAM)0 , (LPARAM)0 );

            break;
        }
        case SB_THUMBPOSITION: {
            gJpegQuality = HIWORD(wParam);

            break;
        }
        case SB_THUMBTRACK: {
            gJpegQuality = HIWORD(wParam);

            break;
        }
        }

        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint( handle , &ps );

        /* ===== Create buffer DC ===== */
        RECT rect;
        GetClientRect( handle , &rect );

        HDC bufferDC = CreateCompatibleDC( hdc );
        HBITMAP bufferBmp = CreateCompatibleBitmap( hdc , rect.right , rect.bottom );

        HBITMAP oldBmp = static_cast<HBITMAP>( SelectObject( bufferDC , bufferBmp ) );
        /* ============================ */

        // Fill buffer DC with a background color
        HRGN region = CreateRectRgn( 0 , 0 , rect.right , rect.bottom );
        FillRgn( bufferDC , region , GetSysColorBrush(COLOR_3DFACE) );
        DeleteObject( region );

        // Creates 1:1 relationship between logical units and pixels
        int oldMapMode = SetMapMode( bufferDC , MM_TEXT );

        BitBlt( hdc , 0 , 0 , rect.right , rect.bottom , bufferDC , 0 , 0 , SRCCOPY );

        // Restore old DC mapping mode
        SetMapMode( bufferDC , oldMapMode );

        // Replace the old bitmap and delete the created one
        DeleteObject( SelectObject( bufferDC , oldBmp ) );

        // Free the buffer DC
        DeleteDC( bufferDC );

        EndPaint( handle , &ps );

        break;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);

        break;
    }

    case WM_MJPEGSTREAM_START: {
        gStreamWinPtr->repaint();

        break;
    }

    case WM_MJPEGSTREAM_STOP: {
        gStreamWinPtr->repaint();

        break;
    }

    case WM_MJPEGSTREAM_NEWIMAGE: {
        gStreamWinPtr->repaint();

        break;
    }

    default: {
        return DefWindowProc(handle, message, wParam, lParam);
    }
    }

    return 0;
}

// Message handler for "About" box
BOOL CALLBACK AboutCbk( HWND hDlg , UINT message , WPARAM wParam , LPARAM lParam ) {
    UNREFERENCED_PARAMETER(lParam);
    switch ( message ) {
    case WM_INITDIALOG: {
        return TRUE;
    }

    case WM_COMMAND: {
        if ( LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL ) {
            EndDialog( hDlg , LOWORD(wParam) );
            return TRUE;
        }

        break;
    }
    }

    return FALSE;
}
