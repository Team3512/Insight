//=============================================================================
//File Name: FindTarget2014.hpp
//Description: Processes a provided image and finds targets like the ones from
//             FRC 2014
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef FIND_TARGET_2014_A_HPP
#define FIND_TARGET_2014_A_HPP

#include "ProcBase.hpp"

class FindTarget2014a : public ProcBase {
public:
	FindTarget2014a();
private:
    void prepareImage();
    void findTargets();
    void clickEvent(int x, int y);

    /* Mouse x and y position */
    int m_mx;
    int m_my;
};

#endif // FIND_TARGET_2014_A_HPP
