# Insight

Insight is a program we wrote which works in conjunction with the DriverStationDisplay (see https://github.com/Team3512/DriverStationDisplay) to perform vision processing on objects relating to the current FRC game using images from an IP camera.

## Dependencies

If one is building on Windows, we recommend using the [MSYS2 compiler](https://msys2.github.io/). The following libraries are required as dependencies and should be installed using either your package manager of choice or MSYS2's pacman:

* Qt 5
* libjpeg-turbo (used for fast JPEG compression/decompression)
* OpenCV (a vision processing library written in C++)

## Building Insight

To build the project, first run `qmake dir` within a terminal from the desired build directory, where "dir" is the relative location of the Insight.pro file. This will generate three makefiles. If a debug build is desired, run `make -f Makefile.Debug`. The default behavior when simply running `make` is to perform a release build.

## Configuration

The IPSettings.txt in the root directory of the project should be kept with the executable since it looks for it in its current directory.

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

#### Miscellaneous

#### `enableImgProcDebug`

This entry can be either 'true' or 'false'. It determines whether images containing the intermediate steps of processing will be written to disk.

#### `overlayPercent`

This entry has a valid range of 0 through 100 inclusive. The slider in Insight's window can be used to adjust the size of the rectangle drawn on the raw image presented in the window and served to clients. This option sets the size of that rectangle when Insight is started. The rectangle will have the same aspect ratio as and be concentric with respect to the image.
