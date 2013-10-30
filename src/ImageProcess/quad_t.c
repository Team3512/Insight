#include <math.h>
#include "quad_t.h"

/* Re-orders the points in a quadrilateral in a counter-clockwise
   manner. */
void
sortquad(struct quad_t *quad_in)
{
    int i;
    struct pointsort_t sortlist[4];

    /* create the array of structs to be sorted */
    for(i = 0; i < 4; i++){
        sortlist[i].point = quad_in->point[i];
        sortlist[i].angle = quad_getangle(quad_in,
            quad_in->point[i]);
    }

    /* sort the list */
    qsort(sortlist, 4, sizeof(struct pointsort_t), quad_sortfunc);

    /* rearrange the input array */
    for(i = 0; i < 4; i++){
        quad_in->point[i] = sortlist[i].point;
    }

    return;
}

/* Determines the angle between point and x-axis, if the origin is in the center
   of the quadrilateral specified by quad. This is used by the sortquad
   function. */
float
quad_getangle(struct quad_t *quad, CvPoint point)
{
    int mpx, mpy;
    int px, py;

    if(quad == NULL)
        return 1;

    /* get the point in the middle of the quadrilateral by
       averaging it's four points */
    mpx = quad->point[0].x;
    mpx += quad->point[1].x;
    mpx += quad->point[2].x;
    mpx += quad->point[3].x;

    mpy = quad->point[0].y;
    mpy += quad->point[1].y;
    mpy += quad->point[2].y;
    mpy += quad->point[3].y;

    mpx /= 4;
    mpy /= 4;

    /* find angle between point and x-axis in relation to center */

    px = point.x;
    py = point.y;

    /* transform the point */
    px -= mpx;
    py -= mpy;

    float angle = atan2(py, px);
    if (px != 0){
        /* second quadrant */
        if (px < 0 && py > 0){
            return 180.f + angle;
        }
        /* third quadrant */
        if (px < 0 && py < 0){
            return 180.f + angle;
        }
        /* fourth quadrant */
        if (px > 0 && py < 0){
            return 360.f + angle;
        }
        return angle;
    }
    else if (py > 0){
        return M_PI_2;
    }
    else if (py < 0){
        return 3.f * M_PI_2;
    }
    else{
        return 0.f;
    }
}

/* The internal sorting function used by qsort(3) as used by
   sortquad() */
int
quad_sortfunc(const void *arg0, const void *arg1)
{
    const struct pointsort_t *quad0 = arg0;
    const struct pointsort_t *quad1 = arg1;

    if (quad0->angle == quad1->angle){
        return -(hypot(quad0->point.x, quad0->point.y) -
            hypot(quad1->point.x, quad1->point.y));
    }
    else {
        return quad0->angle - quad1->angle;
    }
}
