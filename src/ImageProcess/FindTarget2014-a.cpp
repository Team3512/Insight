//=============================================================================
//File Name: FindTarget2014-a.cpp
//Description: Processes a provided image and finds targets like the ones from
//             FRC 2014
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "FindTarget2014-a.hpp"
#include <stdio.h>

FindTarget2014a::FindTarget2014a() {
	m_mx = 0;
	m_my = 0;
}

void FindTarget2014a::findTargets() {
#if 0
    cvCvtColor( m_cvRawImage , m_cvGrayChannel , CV_RGB2GRAY );

    /* Apply binary threshold to all channels
     * (Eliminates cross-hatching artifact in soft blacks)
     */
    cvThreshold( m_cvGrayChannel , m_cvGrayChannel , 128 , 255 , CV_THRESH_BINARY );

    cvDilate( m_cvGrayChannel , m_cvGrayChannel , NULL , 2 );
#endif
}

void FindTarget2014a::prepareImage() {
    /* int b, g, r; */
	int v;
    int x, y;
    IplImage* img = m_cvRawImage;
    IplImage *srcimg;
    IplImage *dstimg;

#if 1
    x = m_mx;
    y = m_my;

    /* b = m_cvRawImage->imageData[img->widthStep * y * x * 3 + 0];
    g = m_cvRawImage->imageData[img->widthStep * y * x * 3 + 1];
    r = m_cvRawImage->imageData[img->widthStep * y * x * 3 + 2]; */

    srcimg = cvCreateImage(cvSize(img->width, img->height), 8, 1);
    dstimg = cvCreateImage(cvSize(img->width, img->height), 8, 1);
    cvCvtColor(img, srcimg, CV_BGR2GRAY);
    //cvAdaptiveThreshold(srcimg, dstimg, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 3, 0);
    cvThreshold( srcimg , dstimg , 128 , 255 , CV_THRESH_BINARY );
    /* cvCvtColor(srcimg, img, CV_GRAY2BGR); */

    /* b = CV_IMAGE_ELEM( img, uchar, y, x * 3);
    g = CV_IMAGE_ELEM( img, uchar, y, (x * 3) + 1);
    r = CV_IMAGE_ELEM( img, uchar, y, (x * 3) + 2); */
    v = CV_IMAGE_ELEM( dstimg, uchar, y, x );

    /* printf("(%d, %d, %d)\n", r, g, b); */
    printf("(%d)\n", v);

#endif

#if 0
    struct quad_t quad;

    CvMemStorage* storage = cvCreateMemStorage( 0 );
    CvContourScanner scanner;
    CvSeq* ctr;

    // Clear list of targets because we found new targets
    m_targets.clear();

    // Find the contours of the targets
    scanner = cvStartFindContours( m_cvGrayChannel , storage , sizeof(CvContour),
            CV_RETR_LIST , CV_CHAIN_APPROX_SIMPLE , cvPoint( 0 , 0 ) );

    while( (ctr = cvFindNextContour(scanner)) != NULL ) {
        // Approximate the polygon, and find the points
        ctr = cvApproxPoly( ctr , sizeof(CvContour) , storage ,
                CV_POLY_APPROX_DP , 10 , 0 );

        // If contour not found or polygon is wrong shape
        if( ctr == NULL || ctr->total != 4 ) {
            continue;
        }

        // Extract the points
        for( int i = 0 ; i < 4 ; i++ ) {
            quad.point[i] = *CV_GET_SEQ_ELEM( CvPoint , ctr , i );
        }

        // Sort the quadrilateral's points counter-clockwise
        sortquad( &quad );

        m_targets.push_back( quad );
    }

    // Clean up from finding our contours
    cvEndFindContours( &scanner );
    cvClearMemStorage( storage );
    cvReleaseMemStorage( &storage );
#endif
}

void FindTarget2014a::clickEvent(int x, int y) {
  printf("clickEvent(%d, %d)\n", x, y);
  m_mx = x;
  m_my = y;
}
