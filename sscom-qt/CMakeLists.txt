cmake_minimum_required(VERSION 3.16)
project(SscomQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

add_executable(sscom-qt
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(sscom-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)

install(TARGETS sscom-qt RUNTIME DESTINATION bin)
