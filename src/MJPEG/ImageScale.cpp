//=============================================================================
//File Name: ImageScale.cpp
//Description: Resizes a given image by either duplicating or skipping pixels
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "ImageScale.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

int
imageScale(
        uint8_t *image_in,
        int width_in,
        int height_in,
        uint8_t *image_out,
        int width_out,
        int height_out,
        int bpp)
{

    double height_in_i;
    double width_in_i;
    double height_out_i;
    double width_out_i;

    int bpp_i;
    double height_unit = ((double)height_in/(double)height_out);
    double width_unit = ((double)width_in/(double)width_out);

    for(height_out_i = 0, height_in_i = 0; height_out_i < height_out; height_out_i++, height_in_i+=height_unit) {
        for(width_out_i = 0, width_in_i = 0; width_out_i < width_out; width_out_i++, width_in_i+=width_unit) {
            for(bpp_i = 0; bpp_i < bpp; bpp_i++) {
                assert(width_out_i <= width_out);
                assert(height_out_i <= height_out);
                assert(bpp_i <= bpp);
                *(image_out+((((int)height_out_i*width_out*bpp)+((int)width_out_i*bpp)))+bpp_i) = *(image_in+((((int)height_in_i*width_in*bpp)+((int)width_in_i*bpp)))+bpp_i);
            }
        }
    }
    return 0;
}
