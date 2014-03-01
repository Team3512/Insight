/*
 * MjpegStreamCallback.hpp
 *
 *  Created on: Feb 28, 2014
 *      Author: Classroom
 */

#ifndef MJPEGSTREAMCALLBACK_HPP_
#define MJPEGSTREAMCALLBACK_HPP_

class MjpegStreamCallback {
public:
	virtual ~MjpegStreamCallback();
	virtual void clickEvent(int x, int y) = 0;
};

#endif /* MJPEGSTREAMCALLBACK_HPP_ */
