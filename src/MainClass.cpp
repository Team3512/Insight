/*
 * MainClass.cpp
 *
 *  Created on: Feb 28, 2014
 *      Author: Classroom
 */

#include "MainClass.hpp"

MainClass::~MainClass() {

}

MainClass::MainClass() {
	m_processor = NULL;
}

void MainClass::clickEvent(int x, int y) {
	m_processor->clickEvent(x, y);
}
