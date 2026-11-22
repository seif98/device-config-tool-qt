cmake_minimum_required(VERSION 3.16)
project(DeviceConfigTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

# GUI-free logic: protocol, parameter table, device simulator
add_library(devcore STATIC
    core/protocol.cpp
    core/device_sim.cpp
)
target_include_directories(devcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DeviceConfigTool
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/Transport.h
    src/SimTransport.h
    src/SimTransport.cpp
    src/SerialTransport.h
    src/SerialTransport.cpp
    src/DeviceClient.h
    src/DeviceClient.cpp
)
target_link_libraries(DeviceConfigTool PRIVATE devcore Qt6::Widgets Qt6::SerialPort)

# Unit tests (no Qt needed): run with  ctest  or the core_tests executable
enable_testing()
add_executable(core_tests tests/test_core.cpp)
target_link_libraries(core_tests PRIVATE devcore)
add_test(NAME core_tests COMMAND core_tests)
