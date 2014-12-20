cmake_minimum_required(VERSION 2.8.8)

set(NAME "Insight")
project(${NAME})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -s -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -s -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ljpeg")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
endif()

set(DIRS
    src/*
)

set(QT_MODULES
    Widgets
    OpenGL
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

foreach(ELEM ${QT_MODULES})
    find_package(Qt5${ELEM} REQUIRED)
endforeach(ELEM)

find_package(OpenCV REQUIRED)

file(GLOB SRC *.cpp *.c)
foreach(ELEM ${DIRS})
    file(GLOB_RECURSE FILES ${ELEM}.cpp ${ELEM}.c)
    list(APPEND SRC ${FILES})
endforeach(ELEM)

file(GLOB RC *.qrc)
foreach(ELEM ${DIRS})
    file(GLOB_RECURSE FILES ${ELEM}.qrc)
    list(APPEND RC ${FILES})
endforeach(ELEM)
QT5_ADD_RESOURCES(RC_SRC ${RC})

add_executable(${NAME} ${SRC} ${RC_SRC})
target_link_libraries(${NAME} ${OpenCV_LIBS})

qt5_use_modules(${NAME} ${QT_MODULES})
