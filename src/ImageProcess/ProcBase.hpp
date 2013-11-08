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
#include <opencv2/core/types_c.h>
#include "quad_t.h"

class ProcBase {
public:
    ProcBase();
    virtual ~ProcBase();

    /* Sets internal image to process
     *
     * The data is copied from the image provided into an internal format, so
     * the pointer isn't retained. An RGB image is expected.
     */
    void setImage( uint8_t* image , uint32_t width , uint32_t height );

    // Processes provided image with the overriden functions
    void processImage();

    // Copies the processed image into the provided buffer (
    void getProcessedImage( uint8_t* buffer );

    // Returns dimensions of processed image
    uint32_t getProcessedWidth();
    uint32_t getProcessedHeight();
    uint32_t getProcessedNumChannels();

    const std::vector<quad_t>& getTargetPositions();

    /* When enabled, PNG images of the intermediate processing steps are saved
     * to disk.
     */
    void enableDebugging( bool enable );

protected:
    // Internal OpenCV primitives
    IplImage* m_cvRawImage; // Raw image
    IplImage* m_cvGrayChannel; // Prepared grayscale channel (output of prepareImage())

    std::vector<quad_t> m_targets;

private:
    bool m_debugEnabled;

    // Override these to process different objects
    virtual void prepareImage() = 0;
    virtual void findTargets() = 0;

    void overlayTargets();
};

#endif // PROC_BASE_HPP
