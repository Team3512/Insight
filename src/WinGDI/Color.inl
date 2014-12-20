//=============================================================================
//File Name: Color.inl
//Description: A utility class for storing OpenGL color data
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

template <class T>
Color<T>::Color() {
    R = 0;
    G = 0;
    B = 0;
    A = 1;
}

template <class T>
Color<T>::Color( T r , T g , T b , T a ) {
    R = r;
    G = g;
    B = b;
    A = a;
}

template <class T>
Color<T>::~Color() {
}

template <class T>
QColor Color<T>::qt() {
    return QColor( R , G , B );
}

template <class T>
T Color<T>::glR() {
    return R / 255;
}

template <class T>
T Color<T>::glG() {
    return G / 255;
}
template <class T>
T Color<T>::glB() {
    return B / 255;
}
template <class T>
T Color<T>::glA() {
    return A / 255;
}
