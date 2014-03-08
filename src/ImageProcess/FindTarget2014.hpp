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
public:
	FindTarget2014();

    /* Returns true if point selected is bright enough to be considered part
     * of a target
     */
    bool foundTarget() const;

private:
    void prepareImage();
    void drawOverlay();
    void clickEvent( int x , int y );

    /* Mouse x and y position */
    int m_mx;
    int m_my;

    bool m_foundTarget;
};

#endif // FIND_TARGET_2014_HPP
