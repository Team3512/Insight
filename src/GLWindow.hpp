//=============================================================================
//File Name: GLWindow.hpp
//Description: Manages OpenGL drawing contexts
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef GL_WINDOW_HPP
#define GL_WINDOW_HPP

#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Vector.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

class GLWindow {
public:
    GLWindow( HWND window );
    virtual ~GLWindow();

    void makeBufferCurrent();
    void swapBuffers();

    // Arguments are buffer dimensions
    void initGL( int x , int y );

    HWND m_window;
    HDC m_dc;
    HGLRC m_threadRC;
};

#endif // GL_WINDOW_HPP
