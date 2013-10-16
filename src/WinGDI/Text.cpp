//=============================================================================
//File Name: Text.cpp
//Description: Provides a wrapper for WinGDI text
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "Text.hpp"
#include "UIFont.hpp"
#include <wingdi.h>

Text::Text( const Vector2i& position , HFONT font , std::wstring text , COLORREF fillColor , COLORREF outlineColor , bool netUpdate ) :
        Drawable( position , Vector2i( 0 , 0 ) , fillColor , outlineColor , 0 ) ,
        m_font( font ) {
    m_string = text;
}

void Text::setFont( HFONT font ) {
    m_font = font;
}

const HFONT Text::getFont() const {
    return m_font;
}

void Text::setString( std::wstring text ) {
    m_string = text;
}

const std::wstring& Text::getString() {
    return m_string;
}

void Text::draw( HDC hdc ) {
    // Select font
    HFONT oldFont;
    if ( m_font != NULL ) {
        oldFont = (HFONT)SelectObject( hdc , m_font );
    }
    else {
        oldFont = (HFONT)SelectObject( hdc , UIFont::getInstance().segoeUI14() );
    }

    // Set text color
    COLORREF oldColor = SetTextColor( hdc , Drawable::getFillColor() );

    // Set text background color
    COLORREF oldBackColor = SetBkColor( hdc , Drawable::getOutlineColor() );

    TextOutW( hdc , getPosition().X , getPosition().Y , m_string.c_str() , static_cast<int>(m_string.length()) );

    // Restore old text color
    SetTextColor( hdc , oldColor );

    // Restore old text background color
    SetBkColor( hdc , oldBackColor );

    // Restore old font
    SelectObject( hdc , oldFont );
}
