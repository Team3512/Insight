//=============================================================================
//File Name: FindTarget2014.hpp
//Description: Processes a provided image and finds targets like the ones from
//             FRC 2014
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef FIND_TARGET_2014_HPP
#define FIND_TARGET_2014_HPP

#include "ProcBase.hpp"

class FindTarget2014 : public ProcBase {
private:
    void prepareImage();
    void findTargets();
};

#endif // FIND_TARGET_2014_HPP
