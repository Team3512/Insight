//=============================================================================
//File Name: UIFont.cpp
//Description: Provides a collection of fonts for use by other classes
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "UIFont.hpp"

UIFont UIFont::m_instance;

UIFont& UIFont::getInstance() {
    return m_instance;
}

UIFont::UIFont() :
    m_segoeUI14( "Segoe UI" , 14 , QFont::DemiBold ) ,
    m_segoeUI18( "Segoe UI" , 18 , QFont::DemiBold )
{

}

const QFont& UIFont::segoeUI14() {
    return m_segoeUI14;
}

const QFont& UIFont::segoeUI18() {
    return m_segoeUI18;
}
