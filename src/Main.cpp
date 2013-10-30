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

// Allows usage of socket in CALLBACK OnEvent
sf::UdpSocket* gCtrlSocketPtr = NULL;

LRESULT CALLBACK OnEvent( HWND handle , UINT message , WPARAM wParam , LPARAM lParam );
BOOL CALLBACK AboutCbk( HWND hDlg , UINT message , WPARAM wParam , LPARAM lParam );

/*
 * Sample routine for JPEG compression.  We assume that the target file name
 * and a compression quality factor are passed in.
 */

GLOBAL(void)
write_JPEG_file (char * filename, int quality, uint8_t* image_data, uint32_t image_width, uint32_t image_height)
{
    /* This struct contains the JPEG compression parameters and pointers to
    * working space (which is allocated as needed by the JPEG library).
    * It is possible to have several such structures, representing multiple
    * compression/decompression processes, in existence at once.  We refer
    * to any one struct (and its associated working data) as a "JPEG object".
    */
    struct jpeg_compress_struct cinfo;
    /* This struct represents a JPEG error handler.  It is declared separately
    * because applications often want to supply a specialized error handler
    * (see the second half of this file for an example).  But here we just
    * take the easy way out and use the standard error handler, which will
    * print a message on stderr and call exit() if compression fails.
    * Note that this struct must live as long as the main JPEG parameter
    * struct, to avoid dangling-pointer problems.
    */
    struct jpeg_error_mgr jerr;
    /* More stuff */
    FILE * outfile;       /* target file */
    JSAMPROW row_pointer[1];  /* pointer to JSAMPLE row[s] */
    int row_stride;       /* physical row width in image buffer */

    /* Step 1: allocate and initialize JPEG compression object */

    /* We have to set up the error handler first, in case the initialization
    * step fails.  (Unlikely, but it could happen if you are out of memory.)
    * This routine fills in the contents of struct jerr, and returns jerr's
    * address which we place into the link field in cinfo.
    */
    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    /* Step 2: specify data destination (eg, a file) */
    /* Note: steps 2 and 3 can be done in either order. */

    /* Here we use the library-supplied code to send compressed data to a
    * stdio stream.  You can also write your own code to do something else.
    * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
    * requires it in order to write binary files.
    */
    if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);

    /* Step 3: set parameters for compression */

    /* First we supply a description of the input image.
    * Four fields of the cinfo struct must be filled in:
    */
    cinfo.image_width = image_width;  /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;       /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;   /* colorspace of input image */
    /* Now use the library's routine to set default compression parameters.
    * (You must set at least cinfo.in_color_space before calling this,
    * since the defaults depend on the source color space.)
    */
    jpeg_set_defaults(&cinfo);
    /* Now you can set any non-default parameters you wish to.
    * Here we just illustrate the use of quality (quantization table) scaling:
    */
    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    /* Step 4: Start compressor */

    /* TRUE ensures that we will write a complete interchange-JPEG file.
    * Pass TRUE unless you are very sure of what you're doing.
    */
    jpeg_start_compress(&cinfo, TRUE);

    /* Step 5: while (scan lines remain to be written) */
    /*           jpeg_write_scanlines(...); */

    /* Here we use the library's state variable cinfo.next_scanline as the
    * loop counter, so that we don't have to keep track ourselves.
    * To keep things simple, we pass one scanline per call; you can pass
    * more if you wish, though.
    */
    row_stride = image_width * 3; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
    * Here the array is only one element long, but you could pass
    * more than one scanline at a time if that's more convenient.
    */
    row_pointer[0] = & image_data[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    /* Step 6: Finish compression */

    jpeg_finish_compress(&cinfo);
    /* After finish_compress, we can close the output file. */
    fclose(outfile);

    /* Step 7: release JPEG compression object */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_compress(&cinfo);

    /* And we're done! */
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

                /* ===== Convert RGBA image to BGR ===== */
                // Copy R, G, and B channels but ignore A channel
                for ( unsigned int posIn = 0, posOut = 0 ; posIn < imgWidth * imgHeight ; posIn++, posOut++ ) {
                    // Copy bytes of pixel into corresponding channels
                    tempImg[3*posOut+0] = imgBuffer[4*posIn+2];
                    tempImg[3*posOut+1] = imgBuffer[4*posIn+1];
                    tempImg[3*posOut+2] = imgBuffer[4*posIn+0];
                }
                /* ===================================== */

                // Process the new image
                processor.setImage( tempImg , imgWidth , imgHeight );
                processor.processImage();

                processor.getProcessedImage( tempImg );

                /* ===== Convert BGR image to RGB ===== */
                for ( unsigned int posIn = 0, posOut = 0 ; posIn < imgWidth * imgHeight * 3 ; posIn += 3 , posOut += 3 ) {
                    // Copy bytes of pixel into corresponding channels
                    tempImg[posOut+0] = tempImg[posIn+2];
                    tempImg[posOut+1] = tempImg[posIn+1];
                    tempImg[posOut+2] = tempImg[posIn+0];
                }
                /* ===================================== */

                // Convert RGB image to JPEG
                write_JPEG_file( "output.jpg" , 100 , tempImg , imgWidth , imgHeight );

                {
                    uint8_t* outputData = NULL;
                    size_t outputLen = 0;

                    FILE* imageFile = fopen( "output.jpg" , "r+b" );
                    if ( imageFile != NULL ) {
                        fseek( imageFile , 0 , SEEK_END );
                        outputLen = ftell( imageFile );

                        outputData = new uint8_t[outputLen];

                        rewind( imageFile );
                        fread( outputData , sizeof(char) , outputLen , imageFile );

                        fclose( imageFile );
                    }

                    // TODO Convert BGR channels to JPEG
                    gServer->serveImage( outputData , outputLen );

                    delete[] outputData;
                }

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
                    gServer->start();
                }

                break;
            }
            case IDM_SERVER_STOP: {
                if ( gStreamWinPtr->isStreaming() ) {
                    // Stop streaming
                    gServer->stop();
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
