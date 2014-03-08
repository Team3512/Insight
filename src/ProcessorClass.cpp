//=============================================================================
//File Name: ProcessorClass.cpp
//Description: Implements callbacks for MjpegStream class with regards to an
//             image processor
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include "ProcessorClass.hpp"
#include "ImageProcess/ProcBase.hpp"

ProcessorClass::ProcessorClass( ProcBase* processor ) {
    m_processor = processor;
}

ProcessorClass::~ProcessorClass() {
}

void ProcessorClass::clickEvent(int x, int y) {
	m_processor->clickEvent( x , y );
}
