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
        sortlist[i].quadrant = quad_getquadrant(quad_in,
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

/* Determines the quadrant point is in, if the origin is in the center
   of the quadrilateral specified by quad. This is used by the sortquad
   function. */
int
quad_getquadrant(struct quad_t *quad, CvPoint point)
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

    /* locate the quadrant point is in */
    px = point.x;
    py = point.y;

    /* transform the point */
    px -= mpx;
    py -= mpy;

    /* this assumes px != 0 && py != 0 */

    /* first quadrant */
    if(px < 0 && py < 0){
        return 0;
    }

    /* second quadrant */
    if(px < 0 && py > 0){
        return 1;
    }

    /* third quadrant */
    if(px > 0 && py > 0){
        return 2;
    }

    /* fourth quadrant */
    if(px > 0 && py < 0){
        return 3;
    }

    return 0;
}

/* The internal sorting function used by qsort(3) as used by
   sortquad() */
int
quad_sortfunc(const void *arg0, const void *arg1)
{
    const struct pointsort_t *quad0 = arg0;
    const struct pointsort_t *quad1 = arg1;

    return quad0->quadrant - quad1->quadrant;
}
