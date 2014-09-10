//=============================================================================
//File Name: Drawable.hpp
//Description: Provides interface for WinGDI drawable objects
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef DRAWABLE_HPP
#define DRAWABLE_HPP

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../Vector.hpp"
#include "Color.hpp"

class Drawable {
public:
    Drawable( const Vector2i& position , const Vector2i& size , Colorf fillColor , Colorf outlineColor , int outlineThickness );
    virtual ~Drawable();

    // Draws the drawable to the currently stored device context
    virtual void draw( HDC hdc ) = 0;

    virtual void setPosition( const Vector2i& position );
    virtual void setPosition( short x , short y );

    const Vector2i getPosition();

    virtual void setSize( const Vector2i& size );
    virtual void setSize( short width , short height );

    const Vector2i getSize();

    virtual void setFillColor( Colorf color );
    Colorf getFillColor();

    virtual void setOutlineColor( Colorf color );
    Colorf getOutlineColor();

    virtual void setOutlineThickness( int thickness );
    int getOutlineThickness();

protected:
    const RECT& getBoundingRect();

private:
    RECT m_boundingRect;

    Colorf m_fillColor;
    Colorf m_outlineColor;
    int m_outlineThickness;
};

#endif // DRAWABLE_HPP
