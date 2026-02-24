cmake_minimum_required(VERSION 3.20)

project(suru LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_subdirectory(libsuru-vm)
add_subdirectory(libsuru)
add_subdirectory(suru)
