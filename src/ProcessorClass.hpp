//=============================================================================
//File Name: ProcessorClass.hpp
//Description: Implements callbacks for MjpegStream class with regards to an
//             image processor
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef PROCESSOR_CLASS_HPP
#define PROCESSOR_CLASS_HPP

#include "MJPEG/MjpegStreamCallback.hpp"

class ProcBase;

class ProcessorClass : public MjpegStreamCallback {
public:
    ProcessorClass( ProcBase* processor );
    virtual ~ProcessorClass();

    // Mouse click event callback
    void clickEvent( int x , int y );

private:
    // The image processor we're using
    ProcBase* m_processor;
};

#endif // PROCESSOR_CLASS_HPP
