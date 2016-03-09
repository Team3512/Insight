QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets opengl

TARGET = Insight
TEMPLATE = app
CONFIG += c++14

SOURCES +=\
    src/MainWindow.cpp \
    src/Main.cpp \
    src/Settings.cpp \
    src/Util.cpp \
    src/ImageProcess/FindTarget2013.cpp \
    src/ImageProcess/FindTarget2014.cpp \
    src/ImageProcess/FindTarget2016.cpp \
    src/ImageProcess/ProcBase.cpp \
    src/MJPEG/ClientBase.cpp \
    src/MJPEG/MjpegClient.cpp \
    src/MJPEG/mjpeg_sck.cpp \
    src/MJPEG/mjpeg_sck_selector.cpp \
    src/MJPEG/MjpegServer.cpp \
    src/MJPEG/VideoStream.cpp \
    src/MJPEG/WebcamClient.cpp \
    src/MJPEG/WpiClient.cpp \
    src/MJPEG/win32_socketpair.c

HEADERS  += \
    src/MainWindow.hpp \
    src/Settings.hpp \
    src/Util.hpp \
    src/ImageProcess/FindTarget2013.hpp \
    src/ImageProcess/FindTarget2014.hpp \
    src/ImageProcess/FindTarget2016.hpp \
    src/ImageProcess/ProcBase.hpp \
    src/MJPEG/ClientBase.hpp \
    src/MJPEG/MjpegClient.hpp \
    src/MJPEG/mjpeg_sck.hpp \
    src/MJPEG/mjpeg_sck_selector.hpp \
    src/MJPEG/MjpegServer.hpp \
    src/MJPEG/VideoStream.hpp \
    src/MJPEG/WebcamClient.hpp \
    src/MJPEG/win32_socketpair.h \
    src/MJPEG/WindowCallbacks.hpp

RESOURCES += \
    Insight.qrc

DISTFILES += \
    Resources.rc

LIBS += -ljpeg -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_videoio
