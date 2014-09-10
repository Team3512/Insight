//=============================================================================
//File Name: GLWindow.cpp
//Description: Manages OpenGL drawing contexts
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "GLWindow.hpp"
#include <iostream>

GLWindow::GLWindow( HWND window ) {
    // Initialize OpenGL window
    m_window = window;

    // Get dimensions of window
    RECT windowPos;
    GetClientRect( m_window , &windowPos );
    Vector2i size( windowPos.right , windowPos.bottom );

    // Stores pixel format
    int format;

    // Set the pixel format for the DC
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support m_window
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
    m_dc = GetDC( m_window );

    // Get the best available match of pixel format for the device context
    format = ChoosePixelFormat( m_dc , &pfd );
    SetPixelFormat( m_dc , format , &pfd );

    // Create the render context (RC)
    m_threadRC = wglCreateContext( m_dc );
}

GLWindow::~GLWindow() {
    wglMakeCurrent( NULL , NULL );
    wglDeleteContext( m_threadRC );
    ReleaseDC( m_window , m_dc );
}

void GLWindow::makeContextCurrent() {
    wglMakeCurrent( m_dc , m_threadRC );
}

void GLWindow::swapBuffers() {
    SwapBuffers( m_dc );
}

void GLWindow::initGL( int x , int y ) {
    GLenum glError;

    /* ===== Initialize OpenGL ===== */
    glClearColor( 1.f , 1.f , 1.f , 1.f );
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

    // Set up screen
    glViewport( 0 , 0 , x , y );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0 , x , y , 0 , -1.f , 1.f );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    // Check for OpenGL errors
    glError = glGetError();
    if( glError != GL_NO_ERROR ) {
        std::cerr << "GLWindow.cpp OpenGL failure: " << gluErrorString( glError ) << "\n";
    }
    /* ============================= */
}
