// =============================================================================
// File Name: FindTarget2013.cpp
// Description: Processes a provided image and finds targets like the ones from
//             FRC 2013
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include <opencv2/imgproc/imgproc.hpp>
#include "FindTarget2013.hpp"

void FindTarget2013::prepareImage() {
    cv::cvtColor(m_rawImage, m_grayChannel, CV_BGR2GRAY);

    /* Apply binary threshold to all channels
     * (Eliminates cross-hatching artifact in soft blacks)
     */
    cv::threshold(m_grayChannel, m_grayChannel, 128, 255, CV_THRESH_BINARY);

    // Perform dilation
    int dilationSize = 2;
    cv::Mat element = getStructuringElement(cv::MORPH_RECT,
                                            cv::Size(2 * dilationSize + 1,
                                                     2 * dilationSize + 1),
                                            cv::Point(dilationSize,
                                                      dilationSize));
    cv::dilate(m_grayChannel, m_grayChannel, element);
}

void FindTarget2013::findTargets() {
    Target target;

    // Clear list of targets because we found new targets
    m_targets.clear();

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    // Find the contours of the targets
    cv::findContours(m_grayChannel,
                     contours,
                     hierarchy,
                     CV_RETR_LIST,
                     CV_CHAIN_APPROX_SIMPLE);

    for (unsigned int i = 0; i < contours.size(); i++) {
        target.clear();
        cv::approxPolyDP(contours[i], target, 10, true);

        if (target.size() == 4) {
            m_targets.push_back(target);
        }
    }
}

void FindTarget2013::drawOverlay() {
    // R , G , B , A
    CvScalar lineColor = cvScalar(0x00, 0xFF, 0x00, 0xFF);

    // Draw lines to show user where the targets are
    for (std::vector<Target>::iterator i = m_targets.begin();
         i != m_targets.end();
         i++) {
        cv::line(m_rawImage, (*i)[0], (*i)[1], lineColor, 2);
        cv::line(m_rawImage, (*i)[1], (*i)[2], lineColor, 2);
        cv::line(m_rawImage, (*i)[2], (*i)[3], lineColor, 2);
        cv::line(m_rawImage, (*i)[3], (*i)[0], lineColor, 2);
    }
}

