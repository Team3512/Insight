// =============================================================================
// Description: Processes a provided image and finds targets like the ones from
//              FRC 2014
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include <opencv2/imgproc/imgproc.hpp>
#include "FindTarget2014.hpp"

void FindTarget2014::clickEvent(int x, int y) {
    m_mx = x;
    m_my = y;
}

void FindTarget2014::prepareImage() {
    cv::cvtColor(m_rawImage, m_grayChannel, CV_BGR2GRAY);

    // cv::adaptiveThreshold(m_grayChannel, m_grayChannel, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 3, 0);
    cv::threshold(m_grayChannel, m_grayChannel, 128, 255, CV_THRESH_BINARY);

    /* A pixel (1 channel) returned from at() here will be either 255 in all
     * channels or 0, which casts to either true or false respectively.
     */
    if (m_grayChannel.at<uint8_t>(m_my, m_mx)) {
        m_foundTarget = true;
    }
    else {
        m_foundTarget = false;
    }
}

void FindTarget2014::drawOverlay() {
    // R , G , B , A
    cv::Scalar lineColor = cvScalar(0x00, 0xFF, 0x00, 0xFF);

    // Determines scale of drawn box compared to full image
    float scale = m_overlayScale;

    // Contain top-left and bottom-right corners of box respectively
    cv::Point box[2];

    // Calculate top-left and bottom-right points
    box[0].x = m_rawImage.cols * (1.f - scale) / 2.f;
    box[0].y = m_rawImage.rows * (1.f - scale) / 2.f;
    box[1].x = m_rawImage.cols * (1.f + scale) / 2.f;
    box[1].y = m_rawImage.rows * (1.f + scale) / 2.f;

    // Draw rectangle with points of box
    cv::rectangle(m_rawImage, box[0], box[1], lineColor, 2);
}

bool FindTarget2014::foundTarget() const {
    return m_foundTarget;
}

void FindTarget2014::setOverlayPercent(const float overlayPercent) {
    m_overlayScale = overlayPercent / 100.f;
}
