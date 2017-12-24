// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#include "FindTarget2013.hpp"

#include <vector>

#include <opencv2/imgproc/imgproc.hpp>

void FindTarget2013::prepareImage() {
    cv::cvtColor(m_rawImage, m_grayChannel, CV_BGR2GRAY);

    /* Apply binary threshold to all channels
     * (Eliminates cross-hatching artifact in soft blacks)
     */
    cv::threshold(m_grayChannel, m_grayChannel, 128, 255, CV_THRESH_BINARY);

    // Perform dilation
    int dilationSize = 2;
    cv::Mat element = getStructuringElement(
        cv::MORPH_RECT, cv::Size(2 * dilationSize + 1, 2 * dilationSize + 1),
        cv::Point(dilationSize, dilationSize));
    cv::dilate(m_grayChannel, m_grayChannel, element);
}

void FindTarget2013::findTargets() {
    Target target;

    // Clear list of targets because we found new targets
    m_targets.clear();

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    // Find the contours of the targets
    cv::findContours(m_grayChannel, contours, hierarchy, CV_RETR_LIST,
                     CV_CHAIN_APPROX_SIMPLE);

    for (auto& contour : contours) {
        target.clear();
        cv::approxPolyDP(contour, target, 10, true);

        if (target.size() == 4) {
            m_targets.push_back(target);
        }
    }
}

void FindTarget2013::drawOverlay() {
    // R , G , B , A
    CvScalar lineColor = cvScalar(0x00, 0xFF, 0x00, 0xFF);

    // Draw lines to show user where the targets are
    for (auto& target : m_targets) {
        cv::line(m_rawImage, target[0], target[1], lineColor, 2);
        cv::line(m_rawImage, target[1], target[2], lineColor, 2);
        cv::line(m_rawImage, target[2], target[3], lineColor, 2);
        cv::line(m_rawImage, target[3], target[0], lineColor, 2);
    }
}
