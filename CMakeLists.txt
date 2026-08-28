cmake_minimum_required(VERSION 3.16)
project(midibutton CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(midibutton
    Sources/StreamDeckMidiButton.cpp
    Sources/base64.cpp
    Sources/Common/main.cpp
    Sources/Common/ESDConnectionManager.cpp
    Sources/Common/ESDLocalizer.cpp
    Sources/Common/ESDUtilitiesLinux.cpp
)

target_include_directories(midibutton PRIVATE
    Sources
    Sources/Common
    Sources/include
    Sources/include/rtmidi17
    Sources/macOS
    Sources/Vendor/asio/include
    Sources/Vendor/websocketpp
)

# Same preprocessor setup as the Xcode project, with the ALSA backend
# replacing CoreAudio (rtmidi17 is used header-only there too).
# ASIO_STANDALONE comes from the force-included pch.h
target_compile_definitions(midibutton PRIVATE
    RTMIDI17_HEADER_ONLY=1
    RTMIDI17_ALSA=1
)

# The Xcode build force-includes the prefix header into every translation
# unit; several sources rely on it for <string>, json and ASIO_STANDALONE.
target_compile_options(midibutton PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/Sources/macOS/pch.h
    -Wno-deprecated-declarations
)

find_package(Threads REQUIRED)
target_link_libraries(midibutton PRIVATE Threads::Threads asound)
