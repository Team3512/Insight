//=============================================================================
//File Name: Main.cpp
//Description: Receives images from the robot, processes them, then forwards
//             them to the DriverStationDisplay
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#define _WIN32_IE 0x0400
#include <commctrl.h>

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <functional>
#include <jpeglib.h>

#include "SFML/System/Clock.hpp"

#include "MJPEG/MjpegStream.hpp"
#include "MJPEG/MjpegServer.hpp"
#include "MJPEG/mjpeg_sck.h"
#include "Settings.hpp"
#include "Resource.h"

#include "ImageProcess/FindTarget2014.hpp"

#include "ProcessorClass.hpp"

// Global because IP configuration settings are needed in CALLBACK OnEvent
Settings gSettings( "IPSettings.txt" );

// Allows manipulation of MjpegStream in CALLBACK OnEvent
MjpegStream* gStreamWinPtr = NULL;
MjpegServer* gServer = NULL;
std::atomic<int> gJpegQuality( 100 );

// Allows usage of socket in CALLBACK OnEvent
std::atomic<mjpeg_socket_t> gCtrlSocket( INVALID_SOCKET );

std::function<void(void)> gNewImageFunc = NULL;

LRESULT CALLBACK OnEvent( HWND handle , UINT message , WPARAM wParam , LPARAM lParam );
BOOL CALLBACK AboutCbk( HWND hDlg , UINT message , WPARAM wParam , LPARAM lParam );

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

    FindTarget2014 processor;
    ProcessorClass processorClass( &processor );

    /* If this isn't allocated on the heap, it can't be destroyed soon enough.
     * If it were allocated on the stack, it would be destroyed when it leaves
     * WinMain's scope, which is after its parent window is destroyed. This
     * causes the cleanup in this object's destructor to not complete
     * successfully.
     */
    gStreamWinPtr = new MjpegStream( gSettings.getString( "streamHost" ) ,
            gSettings.getInt( "streamPort" ) ,
            gSettings.getString( "streamRequestPath" ) ,
            mainWindow ,
            0 ,
            0 ,
            320 ,
            240 ,
            Instance,
            &processorClass );

    gServer = new MjpegServer( gSettings.getInt( "streamServerPort" ) );

    /* ===== Robot Data Sending Variables ===== */
    gCtrlSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if ( mjpeg_sck_valid(gCtrlSocket) ) {
        mjpeg_sck_setnonblocking(gCtrlSocket, 1);
    }
    else {
        std::cout << __FILE__ << ": failed to create robot control socket\n";
    }

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
    char data[9] = "ctrl\r\n\0\0";
    std::memset( data + 8 , 0 , sizeof(data) - 8 );
    bool newData = false;

    uint32_t robotIP = 0;
    std::string robotIPStr = gSettings.getString( "robotIP" );

    if ( robotIPStr == "255.255.255.255" ) {
        robotIP = INADDR_BROADCAST;
    }
    else {
        robotIP = inet_addr( robotIPStr.c_str() );

        if ( robotIP == INADDR_NONE ) {
            // Not a valid address, try to convert it as a host name
            hostent* host = gethostbyname( robotIPStr.c_str() );

            if ( host ) {
                robotIP = reinterpret_cast<in_addr*>(host->h_addr_list[0])->s_addr;
            }
            else {
                // Not a valid address nor a host name
                robotIP = 0;
            }
        }
    }

    unsigned short robotCtrlPort = gSettings.getInt( "robotControlPort" );

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
    /* ====================================== */

    // Image processing debugging is disabled by default
    if ( gSettings.getString( "enableImgProcDebug" ) == "true" ) {
        processor.enableDebugging( true );
    }

    uint8_t* serveImg = NULL;
    unsigned long int serveLen = 0;

    /* ===== Set up libjpeg objects ===== */
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer; // pointer to start of scanline (row of image)

    // Set up the error handler
    cinfo.err = jpeg_std_error( &jerr );

    // Initialize the JPEG compression object
    jpeg_create_compress( &cinfo );

    // Specify data destination (eg, memory buffer)
    jpeg_mem_dest( &cinfo , &serveImg , &serveLen );

    /* First we supply a description of the input image. Four fields of the
     * cinfo struct must be filled in:
     */
    cinfo.image_width = 320;     // image width, in pixels
    cinfo.image_height = 240;   // image height, in pixels
    cinfo.input_components = 3;          // # of color components per pixel
    cinfo.in_color_space = JCS_EXT_BGR;  // colorspace of input image

    // Use the library's routine to set default compression parameters
    jpeg_set_defaults( &cinfo );
    /* ================================== */

    gNewImageFunc = [&]{
        // Get new image to process
        imgBuffer = gStreamWinPtr->getCurrentImage();
        imgWidth = gStreamWinPtr->getCurrentSize().X;
        imgHeight = gStreamWinPtr->getCurrentSize().Y;

        if ( imgBuffer != NULL && imgWidth > 0 && imgHeight > 0 ) {
            if ( tempImg == NULL ) {
                tempImg = new uint8_t[imgWidth * imgHeight * 3];
            }
            else if ( lastWidth * lastHeight != imgWidth * imgHeight ) {
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

            /* Free output buffer for JPEG compression because libjpeg leaks
             * memory from jpeg_mem_dest() if it's not NULL (see libjpeg's
             * jdatadst.c line 242 for the function)
             */
            std::free( serveImg );
            serveImg = NULL;

            jpeg_mem_dest( &cinfo , &serveImg , &serveLen );

            /* ===== Convert RGB image to JPEG ===== */
            cinfo.image_width = imgWidth;
            cinfo.image_height = imgHeight;

            // Set any non-default parameters
            jpeg_set_quality( &cinfo , gJpegQuality , TRUE /* limit to baseline-JPEG values */);

            // TRUE ensures that we will write a complete interchange-JPEG file
            jpeg_start_compress( &cinfo , TRUE );

            /* while (scan lines remain to be written)
             *     jpeg_write_scanlines(...);
             */

            /* Here we use the library's state variable cinfo.next_scanline as
             * the loop counter, so that we don't have to keep track ourselves.
             */
            while ( cinfo.next_scanline < cinfo.image_height ) {
                row_pointer = tempImg + cinfo.next_scanline * cinfo.image_width *
                        cinfo.input_components;
                (void) jpeg_write_scanlines( &cinfo , &row_pointer , 1 );
            }

            jpeg_finish_compress( &cinfo );
            /* ===================================== */

            gServer->serveImage( serveImg , serveLen );

            // Send status on target search to robot
            data[8] = processor.foundTarget();

#if 0
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
#endif
            newData = true;

            // Update width and height
            lastWidth = imgWidth;
            lastHeight = imgHeight;
        }

        if ( mjpeg_sck_valid(gCtrlSocket) && sendTime.getElapsedTime() > 200 &&
                newData ) {
            // Build the target address
            sockaddr_in addr;
            std::memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
            addr.sin_addr.s_addr = htonl(robotIP);
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons(robotCtrlPort);

            int sent = sendto(gCtrlSocket, data, sizeof(data), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

            // Check for errors
            if (sent >= 0) {
                newData = false;
                sendTime.restart();
            }
        }
    };

    // Make sure the main window is shown before continuing
    ShowWindow( mainWindow , SW_SHOW );

    while ( GetMessage( &message , NULL , 0 , 0 ) > 0 ) {
        // If a message was waiting in the message queue, process it
        TranslateMessage( &message );
        DispatchMessage( &message );
    }

    // Delete MJPEG stream window and server
    delete gStreamWinPtr;
    delete gServer;

    jpeg_destroy_compress( &cinfo );

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

        // Processes new images
        gNewImageFunc();

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
