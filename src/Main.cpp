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

#include "SFML/Network/IpAddress.hpp"
#include "SFML/Network/UdpSocket.hpp"

#include "SFML/System/Clock.hpp"

#include "MJPEG/MjpegStream.hpp"
#include "Settings.hpp"
#include "Resource.h"

#include "ImageProcess/FindTarget2013.hpp"

#define _WIN32_IE 0x0400
#include <commctrl.h>

// Global because IP configuration settings are needed in CALLBACK OnEvent
Settings gSettings( "IPSettings.txt" );

// Allows manipulation of MjpegStream in CALLBACK OnEvent
MjpegStream* gStreamWinPtr = NULL;

// Allows usage of socket in CALLBACK OnEvent
sf::UdpSocket* gCtrlSocketPtr = NULL;

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

    const int winWidth = 320;
    const int winHeight = 288;

    // Create a new window to be used for the lifetime of the application
    HWND mainWindow = CreateWindowEx( 0 ,
            mainClassName ,
            "Insight" ,
            WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX | WS_CLIPCHILDREN ,
            ( GetSystemMetrics(SM_CXSCREEN) - winWidth ) / 2 ,
            ( GetSystemMetrics(SM_CYSCREEN) - winHeight ) / 2 ,
            winWidth ,
            winHeight ,
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
            ( winWidth - 320 ) / 2 ,
            0 ,
            320 ,
            240 ,
            Instance );

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

                // Update width and height
                lastWidth = imgWidth;
                lastHeight = imgHeight;

                /* ===== Convert RGBA image to RGB ===== */
                // Copy R, G, and B channels but ignore A channel
                for ( unsigned int posIn = 0, posOut = 0 ; posIn < imgWidth * imgHeight * 4 ; posIn++, posOut++ ) {
                    // copy bit here
                    tempImg[posOut] = imgBuffer[posIn];

                    // Skip 'A' part of RGBA pixel
                    if ( posIn % 4 == 3 ) {
                        posIn++;
                    }
                }
                /* ===================================== */

                // Process the new image
                processor.setImage( tempImg , imgWidth , imgHeight );
                processor.processImage();

                processor.getProcessedImage( tempImg ); // TODO Pass tempImg to MJPEG server

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

    // Delete MJPEG stream window
    delete gStreamWinPtr;

    delete[] tempImg;

    // Clean up windows
    DestroyWindow( mainWindow );
    UnregisterClass( mainClassName , Instance );

    return message.wParam;
}

LRESULT CALLBACK OnEvent( HWND handle , UINT message , WPARAM wParam , LPARAM lParam ) {
    switch ( message ) {
    case WM_COMMAND: {
        /* TODO Use these if necessary
        sf::IpAddress robotIP( gSettings.getValueFor( "robotIP" ) );
        unsigned short robotDataPort = std::atoi( gSettings.getValueFor( "robotControlPort" ).c_str() );
        */

        switch( LOWORD(wParam) ) {
            case IDM_SERVER_START: {
                if ( !gStreamWinPtr->isStreaming() ) {
                    // Start streaming
                    gStreamWinPtr->startStream();
                }

                break;
            }
            case IDM_SERVER_STOP: {
                if ( gStreamWinPtr->isStreaming() ) {
                    // Stop streaming
                    gStreamWinPtr->stopStream();
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
