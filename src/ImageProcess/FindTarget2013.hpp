// =============================================================================
// Description: Processes a provided image and finds targets like the ones from
//             FRC 2013
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#ifndef FIND_TARGET_2013_HPP
#define FIND_TARGET_2013_HPP

#include "ProcBase.hpp"

class FindTarget2013 : public ProcBase {
private:
    void prepareImage();
    void findTargets();
    void drawOverlay();
};

#endif // FIND_TARGET_2013_HPP

