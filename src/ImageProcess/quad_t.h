#ifndef QUAD_T_H
#define QUAD_T_H

#ifdef __cplusplus
extern "C" {
#endif

#include <opencv2/core/types_c.h>

struct quad_t {
    CvPoint point[4];
};

struct pointsort_t {
    CvPoint point;
    float angle;
};

void sortquad(struct quad_t *quad_in);
float quad_getangle(struct quad_t *quad, CvPoint point);
int quad_sortfunc(const void *arg0, const void *arg1);

#ifdef __cplusplus
}
#endif

#endif /* QUAD_T_H */
