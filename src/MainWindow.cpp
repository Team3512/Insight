#include <QtWidgets>
#include <QPushButton>
#include <QSlider>

#include "MainWindow.hpp"

#include <iostream>
#include <cstring>
#include <chrono>

MainWindow::MainWindow() {
    centralWidget = new QWidget( this );
    setCentralWidget( centralWidget );

    settings = new Settings( "IPSettings.txt" );

    streamCallback.clickEvent = [&]( int x , int y ) { processor->clickEvent( x , y ); };
    client = new MjpegStream( settings->getString( "streamHost" ) ,
            settings->getInt( "streamPort" ) ,
            settings->getString( "streamRequestPath" ) ,
            this ,
            320 ,
            240 ,
            &streamCallback ,
            [this] { newImageFunc(); } );

    button = new QPushButton( "Start Stream" );
    button->setGeometry( QRect( QPoint( 5 , 5 ) , QSize( 100 , 28 ) ) );
    connect( button , SIGNAL(released()) , this , SLOT(handleButton()) );

    slider = new QSlider( Qt::Horizontal );
    slider->setRange( 0 , 100 );
    slider->setTickInterval( 20 );
    slider->setTickPosition( QSlider::TicksBelow );
    slider->setSingleStep( 1 );
    slider->setValue( settings->getInt( "overlayPercent" ) );
    connect( slider , SIGNAL(valueChanged(int)) , this , SLOT(handleSlider(int)) );

    QHBoxLayout* serverCtrlLayout = new QHBoxLayout;
    serverCtrlLayout->addWidget( button );
    serverCtrlLayout->addWidget( slider );

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget( client );
    mainLayout->addLayout( serverCtrlLayout );

    centralWidget->setLayout( mainLayout );

    createActions();
    createMenus();

    setUnifiedTitleAndToolBarOnMac( true );

    server = new MjpegServer( settings->getInt( "streamServerPort" ) );
    processor = new FindTarget2014();
    processor->setOverlayPercent( settings->getInt( "overlayPercent" ) );

    // Image processing debugging is disabled by default
    if ( settings->getString( "enableImgProcDebug" ) == "true" ) {
        processor->enableDebugging( true );
    }

    /* ===== Robot Data Sending Variables ===== */
    ctrlSocket = socket( AF_INET , SOCK_DGRAM , 0 );

    if ( mjpeg_sck_valid(ctrlSocket) ) {
        mjpeg_sck_setnonblocking( ctrlSocket , 1 );
    }
    else {
        std::cout << __FILE__ << ": failed to create robot control socket\n";
    }

    std::strcpy( data , "ctrl\r\n\0\0" );
    std::memset( data + 8 , 0 , sizeof(data) - 8 );
    newData = false;

    robotIP = 0;
    robotIPStr = settings->getString( "robotIP" );

    if ( robotIPStr == "255.255.255.255" ) {
        robotIP = INADDR_BROADCAST;
    }
    else {
        robotIP = inet_addr( robotIPStr.c_str() );

        if ( robotIP == INADDR_NONE ) {
            // Not a valid address, try to convert it as a host name
            hostent* host = gethostbyname( robotIPStr.c_str() );

            if ( host ) {
                robotIP = reinterpret_cast<in_addr*>(host->h_addr_list[0])->s_addr;
            }
            else {
                // Not a valid address nor a host name
                robotIP = 0;
            }
        }
    }

    robotCtrlPort = settings->getInt( "robotControlPort" );

    // Make sure control data isn't sent too fast
    std::chrono::time_point<std::chrono::system_clock> lastSendTime;
    /* ======================================== */
}

MainWindow::~MainWindow() {
    // Delete MJPEG stream window and server
    delete client;
    delete server;

    delete[] tempImg;
}

void MainWindow::startServer() {
    client->start();
    server->start();
}

void MainWindow::stopServer() {
    server->stop();
    client->stop();
}

void MainWindow::about() {
    QMessageBox::about( this , tr("About Insight") ,
            tr("<br>Insight, Version 1.0<br>"
               "Copyright &copy;2013 FRC Team 3512<br>"
               "FRC Team 3512<br>"
               "All Rights Reserved") );
}

void MainWindow::handleButton() {
    if ( client->isStreaming() ) {
        stopServer();
        button->setText( "Start Stream" );
    }
    else {
        startServer();
        button->setText( "Stop Stream" );
    }
}

void MainWindow::handleSlider( int value ) {
    slider->setValue( value );
    processor->setOverlayPercent( value );
}

void MainWindow::newImageFunc() {
    // Get new image to process
    imgBuffer = client->getCurrentImage();
    imgWidth = client->getCurrentWidth();
    imgHeight = client->getCurrentHeight();

    if ( imgBuffer != NULL && imgWidth > 0 && imgHeight > 0 ) {
        if ( tempImg == NULL ) {
            tempImg = new uint8_t[imgWidth * imgHeight * 3];
        }
        else if ( lastWidth * lastHeight != imgWidth * imgHeight ) {
            delete[] tempImg;
            tempImg = new uint8_t[imgWidth * imgHeight * 3];
        }

        /* ===== Convert RGBA image to BGR for OpenCV ===== */
        // Copy R, G, and B channels but ignore A channel
        for ( unsigned int posIn = 0, posOut = 0 ; posIn < imgWidth * imgHeight ; posIn++, posOut++ ) {
            // Copy bytes of pixel into corresponding channels
            tempImg[3*posOut+0] = imgBuffer[4*posIn+2];
            tempImg[3*posOut+1] = imgBuffer[4*posIn+1];
            tempImg[3*posOut+2] = imgBuffer[4*posIn+0];
        }
        /* ================================================ */

        // Process the new image
        processor->setImage( tempImg , imgWidth , imgHeight );
        processor->processImage();

        server->serveImage( tempImg , imgWidth , imgHeight );

        // Send status on target search to robot
        data[8] = processor->foundTarget();

#if 0
        // Retrieve positions of targets and send them to robot
        if ( processor->getTargetPositions().size() > 0 ) {
            char x = 0;
            char y = 0;

            // Pack data structure with points
            for ( unsigned int i = 0 ; i < processor->getTargetPositions().size() &&
                    i < 3 ; i++ ) {
                quad_t target = processor->getTargetPositions()[i];
                for ( unsigned int j = 0 ; j < 4 ; j++ ) {
                    x += target.point[j].x;
                    y += target.point[j].y;
                }
                x /= 4;
                y /= 4;

                data[9 + 2*i] = x;
                data[10 + 2*i] = y;
            }

            /* If there are less than three points in the data
             * structure, zero the rest out.
             */
            for ( unsigned int i = processor->getTargetPositions().size() ;
                    i < 3 ; i++ ) {
                data[9 + 2*i] = 0;
                data[10 + 2*i] = 0;
            }

            // We have new target data to send to the robot
            newData = true;
        }
#endif
        newData = true;

        // Update width and height
        lastWidth = imgWidth;
        lastHeight = imgHeight;
    }

    // If socket is valid, data was sent at least 200ms ago, and there is new data
    if ( mjpeg_sck_valid(ctrlSocket) && std::chrono::system_clock::now() - lastSendTime > std::chrono::milliseconds(200) &&
            newData ) {
        // Build the target address
        sockaddr_in addr;
        std::memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
        addr.sin_addr.s_addr = htonl(robotIP);
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(robotCtrlPort);

        int sent = sendto(ctrlSocket, data, sizeof(data), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        // Check for errors
        if ( sent >= 0 ) {
            newData = false;
            lastSendTime = std::chrono::system_clock::now();
        }
    }
};

void MainWindow::createActions() {
    startServerAct = new QAction( tr("&Start") , this );
    connect( startServerAct , SIGNAL(triggered()) , this , SLOT(startServer()) );

    stopServerAct = new QAction( tr("&Stop") , this );
    connect( stopServerAct , SIGNAL(triggered()) , this , SLOT(stopServer()) );

    aboutAct = new QAction( tr("&About Insight") , this );
    connect( aboutAct , SIGNAL(triggered()) , this , SLOT(about()) );
}

void MainWindow::createMenus() {
    serverMenu = menuBar()->addMenu( tr("&Server") );
    serverMenu->addAction( startServerAct );
    serverMenu->addAction( stopServerAct );

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu( tr("&Help") );
    helpMenu->addAction( aboutAct );
}
