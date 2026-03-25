cmake_minimum_required(VERSION 3.16)
project(usb-autodump LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(usb-autodump
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.ui
    src/usb_card.cpp
    src/usb_card.ui
    src/log_panel.cpp
    src/settings_dialog.cpp
    src/settings_dialog.ui
    src/usb_monitor.cpp
    src/copy_engine.cpp
    src/ftp_uploader.cpp
    src/disk_tool.cpp
    src/file_record.cpp
    src/crypto.cpp
    src/config.cpp
)

target_link_libraries(usb-autodump
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)

set_target_properties(usb-autodump PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
