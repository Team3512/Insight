//=============================================================================
//File Name: Vector.hpp
//Description: Declares a container for coordinate pairs
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef VECTOR_HPP
#define VECTOR_HPP

template<class T , class U>
struct Vector {
    Vector( T x , U y ) {
        X = x;
        Y = y;
    }

    T X;
    U Y;
};

typedef Vector<int , int> Vector2i;

#endif // VECTOR_HPP
