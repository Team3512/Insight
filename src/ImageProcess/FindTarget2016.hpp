// =============================================================================
// Description: Processes a provided image and finds targets like the ones from
//             FRC 2016
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#ifndef FIND_TARGET_2016_HPP
#define FIND_TARGET_2016_HPP

#include "ProcBase.hpp"
#include <iostream>

class FindTarget2016 : public ProcBase {
public:
    /* Sets scale of box dimensions compared to image dimensions
     * 'percent' should be a percentage from 0 to 100 inclusive
     */
    void setOverlayPercent(const float percent);

private:
    void prepareImage();
    void findTargets();
    void drawOverlay();

    float m_overlayScale = 1.f;
};

#endif // FIND_TARGET_2016_HPP
