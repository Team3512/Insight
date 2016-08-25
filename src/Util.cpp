// Copyright (c) FRC Team 3512, Spartatroniks 2013-2016. All Rights Reserved.

#include "Util.hpp"

int npot(int num) {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num++;

    return num;
}
