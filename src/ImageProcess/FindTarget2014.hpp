// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#pragma once

#include "ProcBase.hpp"

/**
 * Processes a provided image and finds targets like the ones from FRC 2014
 */
class FindTarget2014 : public ProcBase {
public:
    /* Returns true if point selected is bright enough to be considered part of
     * a target
     */
    bool foundTarget() const;

    /* Sets scale of box dimensions compared to image dimensions
     * 'percent' should be a percentage from 0 to 100 inclusive
     */
    void setOverlayPercent(const float percent);

    void clickEvent(int x, int y);

private:
    void prepareImage();
    void drawOverlay();

    // Mouse x and y position
    int m_mx = 0;
    int m_my = 0;

    bool m_foundTarget = false;

    float m_overlayScale = 1.f;
};
