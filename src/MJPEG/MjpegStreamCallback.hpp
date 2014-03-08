//=============================================================================
//File Name: MjpegStreamCallback.hpp
//Description: Provides callback interface for MjpegStream's window events
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#ifndef MJPEG_STREAM_CALLBACK_HPP
#define MJPEG_STREAM_CALLBACK_HPP

class MjpegStreamCallback {
public:
	virtual ~MjpegStreamCallback();
	virtual void clickEvent(int x, int y) = 0;
};

#endif // MJPEG_STREAM_CALLBACK_HPP
