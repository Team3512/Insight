// Copyright (c) 2013-2018 FRC Team 3512. All Rights Reserved.

#pragma once

#include <functional>

/**
 * Provides callback interface for MjpegStream's window events
 */
class WindowCallbacks {
public:
    // The arguments are 'int x' and 'int y'
    std::function<void(int, int)> clickEvent = [](int, int) {};
};
