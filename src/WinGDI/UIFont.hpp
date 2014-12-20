//=============================================================================
//File Name: UIFont.hpp
//Description: Provides a collection of fonts for use by other classes
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef UIFONT_HPP
#define UIFONT_HPP

#include <QFont>

class UIFont {
public:
    // @return a global instance of the fonts available
    static UIFont& getInstance();

    const QFont& segoeUI14();
    const QFont& segoeUI18();

protected:
    UIFont();

private:
    static UIFont m_instance;

    QFont m_segoeUI14;
    QFont m_segoeUI18;

    // Disallow copy and assignment
    UIFont ( const UIFont& );
    void operator=( const UIFont& );
};

#endif // UIFONT_HPP
