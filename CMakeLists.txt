cmake_minimum_required(VERSION 3.27)
project(MonitorGuard)

set(CMAKE_CXX_STANDARD 17)

find_package(X11 REQUIRED)

add_executable(MonitorGuard main.cpp)

target_link_libraries(MonitorGuard ${X11_LIBRARIES} Xi)
include_directories(${X11_INCLUDE_DIR})
