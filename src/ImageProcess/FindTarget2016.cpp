// =============================================================================
// Description: Processes a provided image and finds targets like the ones from
//             FRC 2013
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include <opencv2/imgproc/imgproc.hpp>
#include "FindTarget2016.hpp"

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
    cv::inRange(m_grayChannel, cv::Scalar(0, 230, 0), cv::Scalar(255,
                                                                 255,
                                                                 255), rgb);

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
        int area = (int) cv::contourArea(contour);
        if (area > maxArea) {
            largeContour = contour;
            maxArea = area;
        }
    }

    if (maxArea == 0) {
        m_centerX = -1;
        m_centerY = -1;
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

int FindTarget2016::getCenterX() {
    return m_center.x;
}

int FindTarget2016::getCenterY() {
    return m_center.y;
}

void FindTarget2016::drawOverlay() {
    // R , G , B , A
    CvScalar lineColor = cvScalar(0xFF, 0x00, 0xFF, 0xFF);

    for (std::vector<Target>::iterator i = m_targets.begin();
         i != m_targets.end();
         i++) {
        // Draw lines to show user where the targets are
        cv::line(m_rawImage, (*i)[0], (*i)[1], lineColor, 3);
        cv::line(m_rawImage, (*i)[1], (*i)[2], lineColor, 3);
        cv::line(m_rawImage, (*i)[2], (*i)[3], lineColor, 3);
        cv::line(m_rawImage, (*i)[3], (*i)[0], lineColor, 3);

        // Draw crosshair
        cv::circle(m_rawImage, m_center, 5, lineColor, 3);
    }
}

void FindTarget2016::setOverlayPercent(const float overlayPercent) {
    m_overlayScale = overlayPercent / 100.f;
}
