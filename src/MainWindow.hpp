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

#include <memory>
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
    void handleSlider(int value);
    void newImageFunc();

private:
    void createActions();
    void createMenus();

    std::unique_ptr<Settings> m_settings;

    WindowCallbacks m_streamCallback;
    MjpegStream* m_client;
    QPushButton* m_button;
    QSlider* m_slider;

    QMenu* m_serverMenu;
    QMenu* m_helpMenu;
    QAction* m_startMJPEGAct;
    QAction* m_stopMJPEGAct;
    QAction* m_aboutAct;

    std::unique_ptr<MjpegServer> m_server;
    std::unique_ptr<FindTarget2014> m_processor;

    /* ===== Image Processing Variables ===== */
    uint8_t* m_imgBuffer = nullptr;
    uint8_t* m_tempImg = nullptr;
    uint32_t m_imgWidth = 0;
    uint32_t m_imgHeight = 0;
    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    /* ====================================== */

    /* ===== Robot Data Sending Variables ===== */
    mjpeg_socket_t m_ctrlSocket;

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
    char m_data[9];

    bool m_newData;
    uint32_t m_robotIP;
    std::string m_robotIPStr;
    unsigned short m_robotCtrlPort;

    // Make sure control data isn't sent too fast
    std::chrono::time_point<std::chrono::system_clock> m_lastSendTime;
    /* ======================================== */
};

#endif // MAIN_WINDOW_HPP

