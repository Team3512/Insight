//=============================================================================
//File Name: FindTarget2013.cpp
//Description: Processes a provided image and finds targets like the ones from
//             FRC 2013
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h> // For contour finding
#include "FindTarget2013.hpp"

void FindTarget2013::prepareImage() {
    cv::cvtColor( m_rawImage , m_grayChannel , CV_RGB2GRAY );

    /* Apply binary threshold to all channels
     * (Eliminates cross-hatching artifact in soft blacks)
     */
    cv::threshold( m_grayChannel , m_grayChannel , 128 , 255 , CV_THRESH_BINARY );

    // Perform dilation
    int dilationSize = 2;
    cv::Mat element = getStructuringElement( cv::MORPH_RECT,
            cv::Size( 2 * dilationSize + 1 , 2 * dilationSize + 1 ),
            cv::Point( dilationSize , dilationSize ) );
    cv::dilate( m_grayChannel , m_grayChannel , element );
}

void FindTarget2013::findTargets() {
    struct quad_t quad;

    CvMemStorage* storage = cvCreateMemStorage( 0 );
    CvContourScanner scanner;
    CvSeq* ctr;

    // Clear list of targets because we found new targets
    m_targets.clear();

    // Find the contours of the targets
    scanner = cvStartFindContours( m_grayChannel.data , storage , sizeof(CvContour),
            CV_RETR_LIST , CV_CHAIN_APPROX_SIMPLE , cv::Point( 0 , 0 ) );

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

void FindTarget2013::drawOverlay() {
    // R , G , B , A
    CvScalar lineColor = cvScalar( 0x00 , 0xFF , 0x00 , 0xFF );

    // Draw lines to show user where the targets are
    for ( std::vector<quad_t>::iterator i = m_targets.begin() ; i != m_targets.end() ; i++ ) {
        cv::line( m_rawImage , i->point[0] , i->point[1] , lineColor , 2 , 8 , 0 );
        cv::line( m_rawImage , i->point[1] , i->point[2] , lineColor , 2 , 8 , 0 );
        cv::line( m_rawImage , i->point[2] , i->point[3] , lineColor , 2 , 8 , 0 );
        cv::line( m_rawImage , i->point[3] , i->point[0] , lineColor , 2 , 8 , 0 );
    }
}
