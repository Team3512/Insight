//=============================================================================
//File Name: MjpegStreamCallback.hpp
//Description: Provides callback interface for MjpegStream's window events
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef MJPEG_STREAM_CALLBACK_HPP
#define MJPEG_STREAM_CALLBACK_HPP

#include <functional>

class MjpegStreamCallback {
public:
	virtual ~MjpegStreamCallback();

	// The arguments are 'int x' and 'int y'
	std::function<void (int,int)> clickEvent;
};

#endif // MJPEG_STREAM_CALLBACK_HPP
