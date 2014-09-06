//=============================================================================
//File Name: WindowCallbacks.hpp
//Description: Provides callback interface for MjpegStream's window events
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef WINDOW_CALLBACKS_HPP
#define WINDOW_CALLBACKS_HPP

#include <functional>

class WindowCallbacks {
public:
    // The arguments are 'int x' and 'int y'
    std::function<void (int,int)> clickEvent;
};

#endif // WINDOW_CALLBACKS_HPP
