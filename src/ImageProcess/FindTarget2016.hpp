// =============================================================================
// Description: Processes a provided image and finds targets like the ones from
//             FRC 2013
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#ifndef FIND_TARGET_2016_HPP
#define FIND_TARGET_2016_HPP

#include "ProcBase.hpp"

class FindTarget2016 : public ProcBase {
private:
    void prepareImage();
    void findTargets();
    void drawOverlay();
};

#endif // FIND_TARGET_20163_HPP
