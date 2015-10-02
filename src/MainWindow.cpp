#include <QtWidgets>
#include <QPushButton>
#include <QSlider>

#include "MainWindow.hpp"

#include <iostream>
#include <cstring>
#include <chrono>

MainWindow::MainWindow() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    m_settings = std::make_unique<Settings>("IPSettings.txt");

    m_streamCallback.clickEvent =
        [&] (int x, int y) { m_processor->clickEvent(x, y); };
    m_client = new MjpegStream(m_settings->getString("streamHost"),
                               m_settings->getInt("streamPort"),
                               m_settings->getString("streamRequestPath"),
                               this,
                               320,
                               240,
                               &m_streamCallback,
                               [this] { newImageFunc(); },
                               [this] { m_button->setText("Stop Stream"); },
                               [this] { m_button->setText("Start Stream"); });

    m_button = new QPushButton("Start Stream");
    connect(m_button, SIGNAL(released()), this, SLOT(toggleButton()));

    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, 100);
    m_slider->setTickInterval(20);
    m_slider->setTickPosition(QSlider::TicksBelow);
    m_slider->setSingleStep(1);
    m_slider->setValue(m_settings->getInt("overlayPercent"));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(handleSlider(int)));

    QHBoxLayout* serverCtrlLayout = new QHBoxLayout;
    serverCtrlLayout->addWidget(m_button);
    serverCtrlLayout->addWidget(m_slider);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_client);
    mainLayout->addLayout(serverCtrlLayout);

    centralWidget->setLayout(mainLayout);

    createActions();
    createMenus();

    setUnifiedTitleAndToolBarOnMac(true);

    m_server =
        std::make_unique<MjpegServer>(m_settings->getInt("streamServerPort"));
    m_processor = std::make_unique<FindTarget2014>();
    m_processor->setOverlayPercent(m_settings->getInt("overlayPercent"));

    // Image processing debugging is disabled by default
    if (m_settings->getString("enableImgProcDebug") == "true") {
        m_processor->enableDebugging(true);
    }

    /* ===== Robot Data Sending Variables ===== */
    m_ctrlSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (mjpeg_sck_valid(m_ctrlSocket)) {
        mjpeg_sck_setnonblocking(m_ctrlSocket, 1);
    }
    else {
        std::cout << __FILE__ << ": failed to create robot control socket\n";
    }

    std::strcpy(m_data, "ctrl\r\n\0\0");
    std::memset(m_data + 8, 0, sizeof(m_data) - 8);
    m_newData = false;

    m_robotIP = 0;
    m_robotIPStr = m_settings->getString("robotIP");

    if (m_robotIPStr == "255.255.255.255") {
        m_robotIP = INADDR_BROADCAST;
    }
    else {
        m_robotIP = inet_addr(m_robotIPStr.c_str());

        if (m_robotIP == INADDR_NONE) {
            // Not a valid address, try to convert it as a host name
            hostent* host = gethostbyname(m_robotIPStr.c_str());

            if (host) {
                m_robotIP =
                    reinterpret_cast<in_addr*>(host->h_addr_list[0])->s_addr;
            }
            else {
                // Not a valid address nor a host name
                m_robotIP = 0;
            }
        }
    }

    m_robotCtrlPort = m_settings->getInt("robotControlPort");
    /* ======================================== */
}

MainWindow::~MainWindow() {
    delete[] m_tempImg;
}

void MainWindow::startMJPEG() {
    m_client->start();
    m_server->start();
}

void MainWindow::stopMJPEG() {
    m_server->stop();
    m_client->stop();
}

void MainWindow::about() {
    QMessageBox::about(this, tr("About Insight"),
                       tr("<br>Insight, Version 2.0<br>"
                          "Copyright &copy;2013-2015 FRC Team 3512<br>"
                          "FRC Team 3512<br>"
                          "All Rights Reserved"));
}

void MainWindow::toggleButton() {
    if (m_client->isStreaming()) {
        stopMJPEG();
    }
    else {
        startMJPEG();
    }
}

void MainWindow::handleSlider(int value) {
    m_slider->setValue(value);
    m_processor->setOverlayPercent(value);
}

void MainWindow::newImageFunc() {
    // Get new image to process
    m_imgBuffer = m_client->getCurrentImage();
    m_imgWidth = m_client->getCurrentWidth();
    m_imgHeight = m_client->getCurrentHeight();

    if (m_imgBuffer != nullptr && m_imgWidth > 0 && m_imgHeight > 0) {
        if (m_tempImg == nullptr) {
            m_tempImg = new uint8_t[m_imgWidth * m_imgHeight * 3];
        }
        else if (m_lastWidth * m_lastHeight != m_imgWidth * m_imgHeight) {
            delete[] m_tempImg;
            m_tempImg = new uint8_t[m_imgWidth * m_imgHeight * 3];
        }

        /* ===== Convert RGB image to BGR for OpenCV ===== */
        // Copy R, G, and B channels but ignore A channel
        for (unsigned int posIn = 0, posOut = 0;
             posIn < m_imgWidth * m_imgHeight;
             posIn++, posOut++) {
            // Copy bytes of pixel into corresponding channels
            m_tempImg[3 * posOut + 0] = m_imgBuffer[3 * posIn + 2];
            m_tempImg[3 * posOut + 1] = m_imgBuffer[3 * posIn + 1];
            m_tempImg[3 * posOut + 2] = m_imgBuffer[3 * posIn + 0];
        }
        /* ================================================ */

        // Process the new image
        m_processor->setImage(m_tempImg, m_imgWidth, m_imgHeight);
        m_processor->processImage();

        m_server->serveImage(m_tempImg, m_imgWidth, m_imgHeight);

        // Send status on target search to robot
        m_data[8] = m_processor->foundTarget();

#if 0
        // Retrieve positions of targets and send them to robot
        if (m_processor->getTargetPositions().size() > 0) {
            char x = 0;
            char y = 0;

            // Pack data structure with points
            for (unsigned int i = 0;
                 i < m_processor->getTargetPositions().size() &&
                 i < 3; i++) {
                quad_t target = m_processor->getTargetPositions()[i];
                for (unsigned int j = 0; j < 4; j++) {
                    x += target.point[j].x;
                    y += target.point[j].y;
                }
                x /= 4;
                y /= 4;

                m_data[9 + 2 * i] = x;
                m_data[10 + 2 * i] = y;
            }

            /* If there are less than three points in the data
             * structure, zero the rest out.
             */
            for (unsigned int i = m_processor->getTargetPositions().size();
                 i < 3; i++) {
                m_data[9 + 2 * i] = 0;
                m_data[10 + 2 * i] = 0;
            }

            // We have new target data to send to the robot
            m_newData = true;
        }
#endif
        m_newData = true;

        // Update width and height
        m_lastWidth = m_imgWidth;
        m_lastHeight = m_imgHeight;
    }

    // If socket is valid, data was sent at least 200ms ago, and there is new data
    if (mjpeg_sck_valid(m_ctrlSocket) &&
        std::chrono::system_clock::now() - m_lastSendTime >
        std::chrono::milliseconds(200) &&
        m_newData) {
        // Build the target address
        sockaddr_in addr;
        std::memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
        addr.sin_addr.s_addr = htonl(m_robotIP);
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(m_robotCtrlPort);

        int sent =
            sendto(m_ctrlSocket, m_data, sizeof(m_data), 0,
                   reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        // Check for errors
        if (sent >= 0) {
            m_newData = false;
            m_lastSendTime = std::chrono::system_clock::now();
        }
    }
}

void MainWindow::createActions() {
    m_startMJPEGAct = new QAction(tr("&Start"), this);
    connect(m_startMJPEGAct, SIGNAL(triggered()), this, SLOT(startMJPEG()));

    m_stopMJPEGAct = new QAction(tr("&Stop"), this);
    connect(m_stopMJPEGAct, SIGNAL(triggered()), this, SLOT(stopMJPEG()));

    m_aboutAct = new QAction(tr("&About Insight"), this);
    connect(m_aboutAct, SIGNAL(triggered()), this, SLOT(about()));
}

void MainWindow::createMenus() {
    m_serverMenu = menuBar()->addMenu(tr("&Server"));
    m_serverMenu->addAction(m_startMJPEGAct);
    m_serverMenu->addAction(m_stopMJPEGAct);

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_aboutAct);
}

