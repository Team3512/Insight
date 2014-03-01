/*
 * MainClass.hpp
 *
 *  Created on: Feb 28, 2014
 *      Author: Classroom
 */

#ifndef MAINCLASS_HPP_
#define MAINCLASS_HPP_

#include "MJPEG/MjpegStreamCallback.hpp"
#include "ImageProcess/ProcBase.hpp"

class MainClass : public MjpegStreamCallback {
public:
	virtual ~MainClass();
	MainClass();

	  /* The image processor we're using */
	  ProcBase *m_processor;

	  /* Mouse click event callback */
	  void clickEvent(int x, int y);
};

#endif /* MAINCLASS_HPP_ */
