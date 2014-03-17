//=============================================================================
//File Name: ProcBase.hpp
//Description: Contains a common collection of functions that must be called
//             to do proper image processing on an object in FRC.
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef PROC_BASE_HPP
#define PROC_BASE_HPP

#include <cstdint>
#include <vector>
#include <opencv2/core/core.hpp>

typedef std::vector<cv::Point> Target;

class ProcBase {
public:
    ProcBase();
    virtual ~ProcBase();

    /* Sets internal image to process
     *
     * The data's pointer is copied and all image processing is done directly
     * to that buffer. Since a shallow copy is used, the buffer must exist for
     * the duration of processing. An RGB image is expected.
     */
    void setImage( uint8_t* image , uint32_t width , uint32_t height );

    // Processes provided image with the overriden functions
    void processImage();

    // Returns buffer used to contain OpenCV image
    uint8_t* getProcessedImage() const;

    // Returns dimensions of processed image
    uint32_t getProcessedWidth() const;
    uint32_t getProcessedHeight() const;
    uint32_t getProcessedNumChannels() const;

    const std::vector<Target>& getTargetPositions() const;

    /* When enabled, PNG images of the intermediate processing steps are saved
     * to disk.
     */
    void enableDebugging( bool enable );

    // Click event
    virtual void clickEvent( int x , int y );

protected:
    // Internal OpenCV primitives
    cv::Mat m_rawImage; // Raw image
    cv::Mat m_grayChannel; // Prepared grayscale channel (output of prepareImage())

    std::vector<Target> m_targets;

private:
    bool m_debugEnabled;

    // Override these to process different objects
    virtual void prepareImage() = 0;
    virtual void findTargets();
    virtual void drawOverlay();
};

#endif // PROC_BASE_HPP
