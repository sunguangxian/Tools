cmake_minimum_required(VERSION 3.16)
project(PortMonitorQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(portmonitor-core STATIC
    src/PcapParser.cpp
    src/PcapParser.h
    src/UsbDecode.cpp
    src/UsbDecode.h
)
target_include_directories(portmonitor-core PUBLIC src)
target_link_libraries(portmonitor-core PUBLIC Qt6::Core)

add_executable(portmonitor-qt WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/CaptureModel.cpp
    src/CaptureModel.h
    src/Style.cpp
    src/Style.h
)
target_link_libraries(portmonitor-qt PRIVATE
    portmonitor-core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)
if(WIN32)
    target_link_libraries(portmonitor-qt PRIVATE advapi32)
endif()

add_executable(portmonitor-scsi-test
    tests/scsi_selftest.cpp
)
target_link_libraries(portmonitor-scsi-test PRIVATE portmonitor-core Qt6::Core)

install(TARGETS portmonitor-qt RUNTIME DESTINATION bin)
