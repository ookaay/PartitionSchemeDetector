cmake_minimum_required(VERSION 3.10)
project(PartitionSchemeDetector CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try pkg-config first
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUSB libusb-1.0)
endif()

# Fallback: locate libusb manually
if(NOT LIBUSB_FOUND)
    find_path(LIBUSB_INCLUDE_DIRS libusb.h
        /usr/include/libusb-1.0
        /usr/local/include/libusb-1.0
        /opt/homebrew/include/libusb-1.0
    )
    find_library(LIBUSB_LIBRARIES usb-1.0
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
    )
    if(LIBUSB_INCLUDE_DIRS AND LIBUSB_LIBRARIES)
        set(LIBUSB_FOUND TRUE)
    endif()
endif()

if(NOT LIBUSB_FOUND)
    message(FATAL_ERROR "libusb-1.0 not found. Install it with your package manager.")
endif()

add_executable(psd app.cpp)
target_include_directories(psd PRIVATE ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(psd PRIVATE ${LIBUSB_LIBRARIES})
