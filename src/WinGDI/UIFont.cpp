//=============================================================================
//File Name: UIFont.cpp
//Description: Provides a collection of fonts for use by other classes
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "UIFont.hpp"
#include <wingdi.h>

UIFont UIFont::m_instance;

UIFont& UIFont::getInstance() {
    return m_instance;
}

UIFont::UIFont() {
    // Font size assumes 1:1 relationship between logical units and pixels
    m_segoeUI14 = CreateFont( -14 ,
            0 ,
            0 ,
            0 ,
            550 ,
            FALSE ,
            FALSE ,
            FALSE ,
            ANSI_CHARSET ,
            OUT_TT_ONLY_PRECIS ,
            CLIP_DEFAULT_PRECIS ,
            CLEARTYPE_QUALITY ,
            FF_DONTCARE | DEFAULT_PITCH ,
            "Segoe UI" );

    m_segoeUI18 = CreateFont( -18 ,
            0 ,
            0 ,
            0 ,
            550 ,
            FALSE ,
            FALSE ,
            FALSE ,
            ANSI_CHARSET ,
            OUT_TT_ONLY_PRECIS ,
            CLIP_DEFAULT_PRECIS ,
            CLEARTYPE_QUALITY ,
            FF_DONTCARE | DEFAULT_PITCH ,
            "Segoe UI" );
}

UIFont::~UIFont() {
    DeleteObject( m_segoeUI14 );
    DeleteObject( m_segoeUI18 );
}

const HFONT UIFont::segoeUI14() {
    return m_segoeUI14;
}

const HFONT UIFont::segoeUI18() {
    return m_segoeUI18;
}
