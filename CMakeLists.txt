cmake_minimum_required(VERSION 3.20)

project(suru LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(BUILD_TESTING)
    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

add_subdirectory(libsuru-vm)
add_subdirectory(libsuru)
add_subdirectory(suru)
