cmake_minimum_required(VERSION 3.17)
project(NDI_VU)

set(CMAKE_CXX_STANDARD 14)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(NDI_INSTALL_DIR "/Library/NDI SDK for Apple" CACHE PATH "Installation directory of NDI SDK")

find_package(NDI REQUIRED)
find_package(Curses REQUIRED)
add_executable(NDI_VU main.cpp)
target_include_directories(NDI_VU PRIVATE ${NDI_INCLUDE_DIRS} ${CURSES_INCLUDE_DIR})
target_link_libraries(NDI_VU PRIVATE ${NDI_LIBRARIES} ${CURSES_LIBRARIES})