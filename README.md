#Insight

Insight is a program we wrote which works in conjunction with the DriverStationDisplay (see https://github.com/Team3512/DriverStationDisplay) to perform vision processing on objects relating to the current FRC game using images from an IP camera.

## Dependencies

This project assumes one is using Eclipse CDT with MinGW GCC as the compiler. The following instructions will explain how to build Insight's dependencies.

You must have the following software installed and working properly:

* Eclipse CDT
* MinGW GCC
* CMake
* Git (should come with MinGW GCC's Msys)

### libjpeg-turbo (used for fast JPEG compression/decompression)

1. Download libjpeg-turbo from http://sourceforge.net/projects/libjpeg-turbo/files/1.3.1/libjpeg-turbo-1.3.1-gcc.exe/download
2. Run the installer and set the extraction directory to C:\MinGW

### OpenCV

OpenCV is a vision processing library written in C++. Since Insight links statically to all dependencies, it needs to be built with special options.

#### Download and installation

Clone the Git repository with:
#### `git clone git://github.com/Itseez/opencv`

##### Run the following commands in MSys shell:
    cd opencv
    git checkout 2.4.9
    mkdir build
    cd build
    cmake-gui ..

Click the "Configure" button and select "MinGW Makefiles" with "Specify native compilers". Click "Next >". Set C to "C:/MinGW/bin/gcc.exe" and C++ to "C:/MinGW/bin/gcc.exe". Click "Finish".
The configuration will fail once. Clicking "Configure" again will complete successfully.

After the configuration is finished, set the following options, then click the "Configure" button followed by the "Generate" button:

* disable `BUILD_JPEG`
* disable `BUILD_OPENEXR`
* disable `BUILD_PERF_TESTS`
* disable `BUILD_SHARED_LIBS`
* disable `BUILD_TESTS`
* set `CMAKE_BUILD_TYPE` to "Release"
* disable `WITH_OPENEXR`

##### Close cmake-gui and run:
    mingw32-make -j2 install

Copy the following files and folders into their respective places in C:/MinGW:

* all files in build/install/x86/mingw/bin to C:/MinGW/bin
* all .a files in build/install/x86/mingw/staticlib to C:/MinGW/lib
* all folders in build/install/include to C:/MinGW/include

## Building Insight

Now import the project into Eclipse CDT and build it. Instructions for this will not be included here.

## Configuration

### IPSettings.txt

The required entries in IPSettings.txt are as follows:

#### `streamHost`

IP address of the MJPEG stream for Insight to process

#### `streamPort`

Port from which the MJPEG stream is served

#### `streamRequestPath`

Path to the MJPEG stream at the given IP address

#### `streamServerPort`

Note: If any one of these settings is incorrect, no MJPEG stream will be displayed or processed. If "streamServerPort" is incorrect, Insight will still work but clients will not be able to receive the processed image.

#### Robot-related Settings

#### `robotIP`

Robot's IP address

#### `robotControlPort`

Port to which to send data relating to the processed image. The data may be target coordinates, drive commands, etc.

#### MIscellaneous

#### `enableImgProcDebug`

This entry can be either 'true' or 'false'. It determines whether images containing the intermediate steps of processing will be written to disk.

#### `overlayPercent`

This entry has a valid range of 0 through 100 inclusive. The slider in Insight's window can be used to adjust the size of the rectangle drawn on the raw image presented in the window and served to clients. This option sets the size of that rectangle when Insight is started. The rectangle will have the same aspect ratio as and be concentric with respect to the image.

###### Example IPSettings.txt
    streamHost         = 10.35.12.11
    streamPort         = 80
    streamRequestPath  = /mjpg/video.mjpg

    streamServerPort   = 8080
    streamServerPath   = /

    #Insight sends to this
    robotIP            = 10.35.12.2
    robotControlPort   = 1130

    #'true' or 'false'
    enableImgProcDebug = false

    #Overlay percent size [0-100]
    overlayPercent     = 38