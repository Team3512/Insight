#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <QMainWindow>

class QPushButton;
class QSlider;
class QMenu;
class QAction;

#include "Settings.hpp"
#include "MJPEG/MjpegStream.hpp"
#include "MJPEG/WindowCallbacks.hpp"
#include "MJPEG/MjpegServer.hpp"
#include "MJPEG/mjpeg_sck.hpp"
#include "ImageProcess/FindTarget2014.hpp"

#include <cstdint>

class MainWindow : public QMainWindow {
    Q_OBJECT
    QWidget* centralWidget;

public:
    MainWindow();
    virtual ~MainWindow();

private slots:
    void startMJPEG();
    void stopMJPEG();
    void about();

    void toggleButton();
    void handleSlider( int value );
    void newImageFunc();

private:
    void createActions();
    void createMenus();

    Settings* settings;

    WindowCallbacks streamCallback;
    MjpegStream* client;
    QPushButton* button;
    QSlider* slider;

    QMenu* serverMenu;
    QMenu* helpMenu;
    QAction* startMJPEGAct;
    QAction* stopMJPEGAct;
    QAction* aboutAct;

    MjpegServer* server;
    FindTarget2014* processor;

    /* ===== Image Processing Variables ===== */
    uint8_t* imgBuffer = NULL;
    uint8_t* tempImg = NULL;
    uint32_t imgWidth = 0;
    uint32_t imgHeight = 0;
    uint32_t lastWidth = 0;
    uint32_t lastHeight = 0;
    /* ====================================== */

    /* ===== Robot Data Sending Variables ===== */
    mjpeg_socket_t ctrlSocket;

    /* Used for sending control packets to robot
     * data format:
     * 8 byte header
     *     "ctrl\r\n\0\0"
     * 6 bytes of x-y pairs
     *     char x
     *     char y
     *     char x
     *     char y
     *     char x
     *     char y
     * 2 empty bytes
     */
    char data[9];

    bool newData;
    uint32_t robotIP;
    std::string robotIPStr;
    unsigned short robotCtrlPort;

    // Make sure control data isn't sent too fast
    std::chrono::time_point<std::chrono::system_clock> lastSendTime;
    /* ======================================== */
};

#endif // MAIN_WINDOW_HPP
