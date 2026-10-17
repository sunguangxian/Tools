cmake_minimum_required(VERSION 3.16)
project(SscomQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

add_executable(sscom-qt
    src/main.cpp
    src/codec.h
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(sscom-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET sscom-qt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    "$<IF:$<CONFIG:Debug>,--debug,--release>"
                    --no-translations
                    "$<TARGET_FILE:sscom-qt>"
            COMMENT "Deploy Qt runtime next to sscom-qt.exe")
    endif()
endif()

install(TARGETS sscom-qt RUNTIME DESTINATION bin)
