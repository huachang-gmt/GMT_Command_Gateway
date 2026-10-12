cmake_minimum_required(VERSION 3.16)

project(GMT_Command_Gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(GMT_Command_Gateway
    src/main.cpp
    src/transport_packet.cpp
    src/usb_transport.cpp
    src/command_parser.cpp
    src/tcp_server.cpp
)

target_include_directories(GMT_Command_Gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
