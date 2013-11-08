//=============================================================================
//File Name: FindTarget2013.cpp
//Description: Processes a provided image and finds targets like the ones from
//             FRC 2013
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "FindTarget2013.hpp"

void FindTarget2013::prepareImage() {
    cvCvtColor( m_cvRawImage , m_cvGrayChannel , CV_RGB2GRAY );

    /* Apply binary threshold to all channels
     * (Eliminates cross-hatching artifact in soft blacks)
     */
    cvThreshold( m_cvGrayChannel , m_cvGrayChannel , 128 , 255 , CV_THRESH_BINARY );

    cvDilate( m_cvGrayChannel , m_cvGrayChannel , NULL , 2 );
}

void FindTarget2013::findTargets() {
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
}
