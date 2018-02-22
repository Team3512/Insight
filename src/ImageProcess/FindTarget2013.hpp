// Copyright (c) 2013-2018 FRC Team 3512. All Rights Reserved.

#pragma once

#include "ProcBase.hpp"

/**
 * Processes a provided image and finds targets like the ones from FRC 2013
 */
class FindTarget2013 : public ProcBase {
private:
    void prepareImage();
    void findTargets();
    void drawOverlay();
};
