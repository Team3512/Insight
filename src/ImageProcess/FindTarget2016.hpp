// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#pragma once

#include <iostream>

#include "ProcBase.hpp"

/**
 * Processes a provided image and finds targets like the ones from FRC 2016
 */
class FindTarget2016 : public ProcBase {
public:
    /* Sets scale of box dimensions compared to image dimensions
     * 'percent' should be a percentage from 0 to 100 inclusive
     */
    void setOverlayPercent(const float percent);

    // Sets the range for green colors that will pass filtering
    void setLowerGreenFilterValue(const float range);

private:
    void prepareImage();
    void findTargets();
    void drawOverlay();

    int m_lowerGreenFilterValue = 230;
    float m_overlayScale = 1.f;
};
