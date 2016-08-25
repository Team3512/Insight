// Copyright (c) FRC Team 3512, Spartatroniks 2013-2016. All Rights Reserved.

#include "FindTarget2016.hpp"

#include <opencv2/imgproc/imgproc.hpp>

void FindTarget2016::prepareImage() {
    // remove noise by eroding and dilating twice
    cv::erode(m_rawImage, m_grayChannel, cv::Mat());
    cv::erode(m_grayChannel, m_grayChannel, cv::Mat());
    cv::dilate(m_grayChannel, m_grayChannel, cv::Mat());
    cv::dilate(m_grayChannel, m_grayChannel, cv::Mat());
}

void FindTarget2016::findTargets() {
    m_targets.clear();

    // Filter green contours
    cv::Mat rgb;
    cv::inRange(m_grayChannel, cv::Scalar(0, m_lowerGreenFilterValue, 0),
                cv::Scalar(200, 255, 200), rgb);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    // Find the countours
    cv::findContours(rgb, contours, hierarchy, cv::RETR_TREE,
                     cv::CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<cv::Point>> filtered;

    // Filter contours with a small perimeter
    for (auto& contour : contours) {
        auto perimeter = arcLength(contour, true);
        if (perimeter > 100 && perimeter < 350) {
            filtered.emplace_back(contour);
        }
    }

    // Keep only the largest filtered contours
    std::vector<cv::Point> largeContour;
    int maxArea = 0;
    for (auto& contour : filtered) {
        int area = static_cast<int>(cv::contourArea(contour));
        if (area > maxArea) {
            largeContour = contour;
            maxArea = area;
        }
    }

    if (maxArea == 0) {
        m_targets = filtered;
        return;
    }

    // simplify large contours
    cv::approxPolyDP(cv::Mat(largeContour), largeContour, 5, true);

    // convex hull
    std::vector<cv::Point> convexHull;
    cv::convexHull(largeContour, convexHull, false);

    // Save contours at this point so we have a target
    m_targets.emplace_back(largeContour);

    // find center of mass
    cv::Moments mo = cv::moments(convexHull);
    m_center = cv::Point(mo.m10 / mo.m00, mo.m01 / mo.m00);
}

void FindTarget2016::drawOverlay() {
    // R , G , B , A
    CvScalar lineColor = cvScalar(0x00, 0x00, 0xFF, 0xFF);

    for (auto& target : m_targets) {
        // Draw lines to show user where the targets are
        cv::line(m_rawImage, target[0], target[1], lineColor, 3);
        cv::line(m_rawImage, target[1], target[2], lineColor, 3);
        cv::line(m_rawImage, target[2], target[3], lineColor, 3);
        cv::line(m_rawImage, target[3], target[0], lineColor, 3);

        // Draw target
        cv::circle(m_rawImage, m_center, 5, lineColor, 3);
    }

    // Draw crosshair horizontal
    cv::line(m_rawImage, cv::Point(m_rawImage.cols / 2, m_rawImage.rows / 2),
             cv::Point(m_rawImage.cols / 2 + 20, m_rawImage.rows / 2),
             lineColor, 4);
    cv::line(m_rawImage, cv::Point(m_rawImage.cols / 2, m_rawImage.rows / 2),
             cv::Point(m_rawImage.cols / 2 - 20, m_rawImage.rows / 2),
             lineColor, 4);

    // Draw crosshair horizontal
    cv::line(m_rawImage, cv::Point(m_rawImage.cols / 2, m_rawImage.rows / 2),
             cv::Point(m_rawImage.cols / 2, m_rawImage.rows / 2 + 20),
             lineColor, 4);
    cv::line(m_rawImage, cv::Point(m_rawImage.cols / 2, m_rawImage.rows / 2),
             cv::Point(m_rawImage.cols / 2, m_rawImage.rows / 2 - 20),
             lineColor, 4);
}
void FindTarget2016::setLowerGreenFilterValue(const float range) {
    m_lowerGreenFilterValue = (range / 100.f) * 255;
}

void FindTarget2016::setOverlayPercent(const float overlayPercent) {
    m_overlayScale = overlayPercent / 100.f;
}
