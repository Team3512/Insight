// Copyright (c) FRC Team 3512, Spartatroniks 2013-2016. All Rights Reserved.

#include "ClientBase.hpp"

void ClientBase::setObject(VideoStream* object) { m_object = object; }

void ClientBase::setNewImageCallback(
    void (VideoStream::*newImageCbk)(uint8_t* buf, int bufsize)) {
    m_newImageCbk = newImageCbk;
}

void ClientBase::setStartCallback(void (VideoStream::*startCbk)()) {
    m_startCbk = startCbk;
}

void ClientBase::setStopCallback(void (VideoStream::*stopCbk)()) {
    m_stopCbk = stopCbk;
}

void ClientBase::callNewImage(uint8_t* buf, int bufsize) {
    (m_object->*m_newImageCbk)(buf, bufsize);
}

void ClientBase::callStart() { (m_object->*m_startCbk)(); }

void ClientBase::callStop() { (m_object->*m_stopCbk)(); }
