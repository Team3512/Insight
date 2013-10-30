//=============================================================================
//File Name: ProcBase.cpp
//Description: Contains a common collection of functions that must be called
//             to do proper image processing on an object in FRC.
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <cstddef>
#include <cstring>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "ProcBase.hpp"

ProcBase::ProcBase() :
        m_cvRawImage( NULL ) ,
        m_cvGrayChannel( NULL ) {

}

ProcBase::~ProcBase() {
    if ( m_cvRawImage != NULL ) {
        cvReleaseImage( &m_cvRawImage );
    }

    if ( m_cvGrayChannel != NULL ) {
        cvReleaseImage( &m_cvGrayChannel );
    }
}

void ProcBase::setImage( uint8_t* image , uint32_t width , uint32_t height ) {
    // Free old image if there is one
    if ( m_cvRawImage != NULL ) {
        cvReleaseImage( &m_cvRawImage );
    }
    if ( m_cvGrayChannel != NULL ) {
        cvReleaseImage( &m_cvGrayChannel );
    }

    CvSize size;
    size.width = width;
    size.height = height;

    // Create new image and store data from provided image into it
    m_cvRawImage = cvCreateImage( size , IPL_DEPTH_8U , 3 );
    std::memcpy( m_cvRawImage->imageData , image , width * height * 3 );

    // Used later after image is processed
    m_cvGrayChannel = cvCreateImage( size , IPL_DEPTH_8U , 1 );
}

void ProcBase::processImage() {
    prepareImage();
    findTargets();
    overlayTargets();

    if ( m_cvRawImage != NULL ) {
        cvSaveImage( "processedImage.png" , m_cvRawImage , NULL ); // TODO Remove me
    }
}

void ProcBase::getProcessedImage( uint8_t* buffer ) {
    if ( m_cvRawImage != NULL ) {
        std::memcpy( buffer , m_cvRawImage->imageData , m_cvRawImage->width * m_cvRawImage->height * m_cvRawImage->nChannels );
    }
}

uint32_t ProcBase::getProcessedWidth() {
    if ( m_cvRawImage != NULL ) {
        return m_cvRawImage->width;
    }
    else {
        return 0;
    }
}

uint32_t ProcBase::getProcessedHeight() {
    if ( m_cvRawImage != NULL ) {
        return m_cvRawImage->height;
    }
    else {
        return 0;
    }
}

uint32_t ProcBase::getProcessedNumChannels() {
    if ( m_cvRawImage != NULL ) {
        return m_cvRawImage->nChannels;
    }
    else {
        return 0;
    }
}

const std::vector<quad_t>& ProcBase::getTargetPositions() {
    return m_targets;
}

void ProcBase::overlayTargets() {
    // R , G , B , A
    CvScalar lineColor = cvScalar( 0x00 , 0xFF , 0x00 , 0xFF );

    // Draw lines to show user where the targets are
    for ( std::vector<quad_t>::iterator i = m_targets.begin() ; i != m_targets.end() ; i++ ) {
        cvLine( m_cvRawImage , i->point[0] , i->point[1] , lineColor , 2 , 8 , 0 );
        cvLine( m_cvRawImage , i->point[1] , i->point[2] , lineColor , 2 , 8 , 0 );
        cvLine( m_cvRawImage , i->point[2] , i->point[3] , lineColor , 2 , 8 , 0 );
        cvLine( m_cvRawImage , i->point[3] , i->point[0] , lineColor , 2 , 8 , 0 );
    }
}
