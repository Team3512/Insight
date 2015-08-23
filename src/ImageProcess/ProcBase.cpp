// =============================================================================
// File Name: ProcBase.cpp
// Description: Contains a common collection of functions that must be called
//             to do proper image processing on an object in FRC.
// Author: FRC Team 3512, Spartatroniks
// =============================================================================

#include <opencv2/highgui/highgui.hpp>
#include "ProcBase.hpp"

void ProcBase::setImage(uint8_t* image, uint32_t width, uint32_t height) {
    // Create new image and store data from provided image into it
    m_rawImage = cv::Mat(height, width, CV_8UC(3), image);

    // Used later after image is processed
    m_grayChannel.create(width, height, CV_8UC(1));
}

void ProcBase::processImage() {
    if (m_debugEnabled) {
        cv::imwrite("rawImage.png", m_rawImage);
    }

    prepareImage();
    if (m_debugEnabled) {
        cv::imwrite("preparedImage.png", m_grayChannel);
    }

    findTargets();

    drawOverlay();
    if (m_debugEnabled) {
        cv::imwrite("processedImage.png", m_rawImage);
    }
}

uint8_t* ProcBase::getProcessedImage() const {
    return m_rawImage.data;
}

uint32_t ProcBase::getProcessedWidth() const {
    return m_rawImage.cols;
}

uint32_t ProcBase::getProcessedHeight() const {
    return m_rawImage.rows;
}

uint32_t ProcBase::getProcessedNumChannels() const {
    return m_rawImage.channels();
}

const std::vector<Target>& ProcBase::getTargetPositions() const {
    return m_targets;
}

void ProcBase::enableDebugging(bool enable) {
    m_debugEnabled = enable;
}

void ProcBase::findTargets() {
}

void ProcBase::drawOverlay() {
}

// Empty clickEvent() function in case not overridden
void ProcBase::clickEvent(int x, int y) {
}

