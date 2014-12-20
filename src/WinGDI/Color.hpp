//=============================================================================
//File Name: Color.hpp
//Description: A utility class for storing OpenGL color data
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef COLOR_HPP
#define COLOR_HPP

#include <QColor>

template <class T>
class Color {
public:
    Color();
    Color( T r , T g , T b , T a = 1 );

    virtual ~Color();

    // Returns color in Qt format
    QColor qt();

    // Normalizes color component to OpenGL's float color range [0..1]
    T glR();
    T glG();
    T glB();
    T glA();

private:
    T R;
    T G;
    T B;
    T A;
};

typedef Color<float> Colorf;
typedef Color<unsigned int> Coloru;
typedef Color<int> Colori;

#include "Color.inl"

#endif // COLOR_HPP
