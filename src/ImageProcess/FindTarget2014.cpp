//=============================================================================
//File Name: FindTarget2014.cpp
//Description: Processes a provided image and finds targets like the ones from
//             FRC 2014
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <opencv2/imgproc/imgproc_c.h>
#include "FindTarget2014.hpp"
#include <stdio.h>

FindTarget2014::FindTarget2014() {
	m_mx = 0;
	m_my = 0;
	m_foundTarget = false;
}

void FindTarget2014::prepareImage() {
    /* int b, g, r; */
    int x, y;

    x = m_mx;
    y = m_my;

    cvCvtColor( m_cvRawImage , m_cvGrayChannel , CV_RGB2GRAY );

    /* b = m_cvRawImage->imageData[m_cvRawImage->widthStep * y * x * 3 + 0];
    g = m_cvRawImage->imageData[m_cvRawImage->widthStep * y * x * 3 + 1];
    r = m_cvRawImage->imageData[m_cvRawImage->widthStep * y * x * 3 + 2]; */

    //cvAdaptiveThreshold(m_cvGrayChannel, m_cvGrayChannel, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 3, 0);
    cvThreshold( m_cvGrayChannel , m_cvGrayChannel , 128 , 255 , CV_THRESH_BINARY );
    //cvCvtColor( m_cvGrayChannel, m_cvRawImage , CV_GRAY2BGR );

    /* b = CV_IMAGE_ELEM( m_cvRawImage, uchar, y, x * 3);
    g = CV_IMAGE_ELEM( m_cvRawImage, uchar, y, (x * 3) + 1);
    r = CV_IMAGE_ELEM( m_cvRawImage, uchar, y, (x * 3) + 2); */
    m_foundTarget = CV_IMAGE_ELEM( m_cvGrayChannel, uchar, y, x );

    /* printf("(%d, %d, %d)\n", r, g, b); */
    printf("(%d)\n", static_cast<int>(m_foundTarget));
}

void FindTarget2014::findTargets() {
}

void FindTarget2014::clickEvent( int x , int y ) {
    printf("clickEvent(%d, %d)\n", x, y);
    m_mx = x;
    m_my = y;
}

bool FindTarget2014::foundTarget() const {
    return m_foundTarget;
}
