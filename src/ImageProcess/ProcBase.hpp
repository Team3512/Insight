// Copyright (c) 2013-2018 FRC Team 3512. All Rights Reserved.

#pragma once

#include <stdint.h>

#include <vector>

#include <opencv2/core/core.hpp>

typedef std::vector<cv::Point> Target;

/**
 * Contains a common collection of functions that must be called to do proper
 * image processing on an object in FRC.
 */
class ProcBase {
public:
    virtual ~ProcBase() = default;

    /* Sets internal image to process
     *
     * The data's pointer is copied and all image processing is done directly
     * to that buffer. Since a shallow copy is used, the buffer must exist for
     * the duration of processing. An RGB image is expected.
     */
    void setImage(uint8_t* image, uint32_t width, uint32_t height);

    // Processes provided image with the overriden functions
    void processImage();

    // Returns buffer used to contain OpenCV image
    uint8_t* getProcessedImage() const;

    // Returns dimensions of processed image
    uint32_t getProcessedWidth() const;
    uint32_t getProcessedHeight() const;
    uint32_t getProcessedNumChannels() const;

    const std::vector<Target>& getTargetPositions() const;

    // Returns the center mass x-coord of the goal
    int getCenterX() const;

    // Returns the center mass x-coord of the goal
    int getCenterY() const;

    /* When enabled, PNG images of the intermediate processing steps are saved
     * to disk.
     */
    void enableDebugging(bool enable);

    // Click event
    virtual void clickEvent(int x, int y);

protected:
    // Raw image
    cv::Mat m_rawImage;

    // Prepared grayscale channel (output of prepareImage())
    cv::Mat m_grayChannel;

    std::vector<Target> m_targets;

    // Returns center of mass of goal
    cv::Point m_center{-1, -1};

private:
    bool m_debugEnabled = false;

    // Override these to process different objects
    virtual void prepareImage() = 0;
    virtual void findTargets();
    virtual void drawOverlay();
};
