// Copyright (c) FRC Team 3512, Spartatroniks 2013-2016. All Rights Reserved.

#ifndef FIND_TARGET_2016_HPP
#define FIND_TARGET_2016_HPP

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

#endif  // FIND_TARGET_2016_HPP
